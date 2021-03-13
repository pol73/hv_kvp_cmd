/* Copyright (c) 2021 "Pavel I Volkov" <pavelivolkov@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE. */

/* References to documentations:
[Hyper-V Data Exchange Service (KVP)]: https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-data-exchange-service-kvp
[Data Exchange: Using key-value pairs to share information between the host and guest on Hyper-V]: https://docs.microsoft.com/en-us/previous-versions/windows/it-pro/windows-server-2012-R2-and-2012/dn798287(v=ws.11)
*/

#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <iconv.h>
#include <langinfo.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <machine/param.h>
#include <dev/hyperv/hyperv.h>
#include "/usr/src/sys/dev/hyperv/utilities/hv_kvp.h"

/* definitions from file: /usr/src/contrib/hyperv/tools/hv_kvp_daemon.c */
#ifndef POOL_DIR
#define POOL_DIR "/var/db/hyperv/pool/"
#endif

#ifndef POOL_NAME
#define POOL_NAME ".kvp_pool_"
#endif
/* end definitions from file: /usr/src/contrib/hyperv/tools/hv_kvp_daemon.c */

#ifndef QUOTECHAR
#define QUOTECHAR '"'
#endif

#ifndef EOL
#define EOL "\n"
#endif

#ifndef MULTIBYTE
#define MULTIBYTE 4
#endif

struct kvp_record {
  char key[HV_KVP_EXCHANGE_MAX_KEY_SIZE];
  char value[HV_KVP_EXCHANGE_MAX_VALUE_SIZE];
};
struct kvp_record_mb { /* destination for convertion kvp_record to or from locale source */
  char key[HV_KVP_EXCHANGE_MAX_KEY_SIZE * MULTIBYTE];
  char value[HV_KVP_EXCHANGE_MAX_VALUE_SIZE * MULTIBYTE];
};

typedef enum {
  READ = 0,
  WRITE,
  REMOVE,
  COMMAND_COUNT
} COMMAND;

int findKeyArgs(int argc, char *argv[], int optind, int iteratorSize, char *key) {
  assert(iteratorSize==1 /* key */ || iteratorSize==2 /* pairs: key value */);
  if(optind < argc) {
    for(; optind<argc; optind+=iteratorSize) {
      if(strlen(argv[optind]) == strlen(key) && strcasestr(argv[optind],key) != NULL) return optind; /* key found (TRUE) */
    }
    return 0; /* key not found (FALSE) */
  }
  return -1; /* all key allowed (TRUE) */
}

void localeConvert(iconv_t codetable, char *src, char *dst, size_t dstlen, int verbose) {
  size_t invalidCharNumbers, srcleft=strlen(src), dstleft=dstlen;
  invalidCharNumbers = iconv(codetable,&src,&srcleft,&dst,&dstleft);
  if(invalidCharNumbers == (size_t)(-1)) errx(EX_DATAERR,"KVP cannot be converted with the iconv");
  if(verbose && invalidCharNumbers > 0) warnx("KVP key have %ld don't convertable characters",invalidCharNumbers);
}

int poolOpen(int pool, int flags /* O_RDONLY | O_RDWR */, int lockType /* F_RDLCK | F_WRLCK */) {
  int fd;
  char poolName[PATH_MAX];
  struct flock fl={0,0,0,lockType,SEEK_SET,0};
  assert(pool>=0 && pool<HV_KVP_POOL_COUNT);
  assert(flags==O_RDONLY || flags==O_RDWR);
  assert(lockType==F_RDLCK || lockType==F_WRLCK);
  if(snprintf(poolName,PATH_MAX-1,"%s%s%d",POOL_DIR,POOL_NAME,pool) < 0) errx(EX_SOFTWARE,"Cannot create pool path variable");
  if((fd=open(poolName,flags)) == -1) err(EX_NOINPUT,"%s",poolName);
  fl.l_pid=getpid();
  if(fcntl(fd,F_SETLKW,&fl) == -1) err(EX_IOERR,"Cannot set lock");
  return fd;
}

void poolClose(int fd) {
  struct flock fl={0,0,0,F_UNLCK,SEEK_SET,0};
  fl.l_pid=getpid();
  if(fcntl(fd,F_SETLK,&fl) == -1) err(EX_IOERR,"Cannot release lock");
  if(close(fd) == -1) err(EX_IOERR,"Cannot close the pool file");
}

int readKVPrecord(int fd, struct kvp_record_mb *kvpDst, iconv_t codebase, int verbose) {
  ssize_t count;
  struct kvp_record kvp={.key="\0",.value="\0"};
  memset(kvpDst,0,sizeof(*kvpDst));
  count=read(fd,&kvp,sizeof(kvp));
  if(count == sizeof(kvp)) {
    localeConvert(codebase,kvp.key,kvpDst->key,sizeof(kvpDst->key),verbose);
    localeConvert(codebase,kvp.value,kvpDst->value,sizeof(kvpDst->value),verbose);
    return 1; /* TRUE */
  }
  if(count == -1) err(EX_IOERR,NULL);
  if(count > 0 /* don't EOF */ && verbose) warnx("KVP record damaged");
  return 0; /* FALSE */
}

void writeKVPrecord(int fd, char *key, char *value, iconv_t codebase, int verbose) {
  struct kvp_record kvp;
  if(strlen(key)>0 && strlen(value)>0) { /* Write key or value cannot be empty */
    memset(&kvp,0,sizeof(kvp));
    localeConvert(codebase,key,kvp.key,sizeof(kvp.key),verbose);
    localeConvert(codebase,value,kvp.value,sizeof(kvp.value),verbose);
    if(write(fd,&kvp,sizeof(kvp)) == -1) err(EX_IOERR,"Cannot write key '%s' to pool",kvp.key);
  }
}

int main(int argc, char *argv[]) {
  int verbose = 0; /* option, verbose output, FALSE by default */
  int quoting = 0; /* option, key and value quoted with QUOTECHAR, FALSE by default */
  char delimiter = '='; /* delimiter between key and his value */
  COMMAND command = READ; /* read key, by default */
  int fd; /* file descriptor for current working pool */
  struct kvp_record_mb kvpDst;
  err_set_file(stderr);

  char * locale = setlocale(LC_ALL,getenv("LANG"));
  if(locale == NULL) errx(EX_SOFTWARE,"Cannot get system locale");
  char * codeset = nl_langinfo(CODESET);
  if(strlen(codeset) == 0) errx(EX_SOFTWARE,"Cannot get system codeset");
  iconv_t ctFromKVP = iconv_open(codeset,"UTF-8");
  if(ctFromKVP == (iconv_t)(-1)) errx(EX_SOFTWARE,"Cannot create the iconv codetable");
  iconv_t ctToKVP = iconv_open("UTF-8",codeset);
  if(ctToKVP == (iconv_t)(-1)) errx(EX_SOFTWARE,"Cannot create the iconv codetable");

  char opt;
  while((opt = getopt(argc, argv, "vqd:wrh")) != -1) {
    switch(opt) {
      case 'v': /* verbose mode */
        verbose = 1;
        break;
      case 'q': /* quoting key and value with quotes mark */
        quoting = 1;
        break;
      case 'd': /* change delimiter to this char */
        delimiter = (char)optarg[0]; /* TODO: don't work with wchar_t */
        break;
      case 'w': /* write key=value */
        command = WRITE;
        break;
      case 'r': /* remove key ... */
        command = REMOVE;
        break;
      case 'h':
      default: errx(EX_USAGE,"usage: %s [-vqwr] [-d char] [key value] or [keys ...]\nlocale(%s), codeset(%s)",strrchr(argv[0],'/')+sizeof(argv[0][0]),locale,codeset);
    }
  }

  switch(command) {
    case READ:
      for(int pool=0; pool<HV_KVP_POOL_COUNT; pool++) { /* for each pool */
        fd=poolOpen(pool,O_RDONLY,F_RDLCK);
        while(readKVPrecord(fd,&kvpDst,ctFromKVP,verbose)) { /* for each key */
          if(findKeyArgs(argc,argv,optind,1,kvpDst.key)) { /* matching by key */
            /* print kvp */
            if(verbose) {
              if(quoting) printf("%c",QUOTECHAR);
              printf("%s%s%d",POOL_DIR,POOL_NAME,pool);
              if(quoting) printf("%c",QUOTECHAR);
              printf("%c",delimiter);
            }
            if(quoting) printf("%c",QUOTECHAR);
            printf("%s",kvpDst.key);
            if(quoting) printf("%c",QUOTECHAR);
            printf("%c",delimiter);
            if(quoting) printf("%c",QUOTECHAR);
            printf("%s",kvpDst.value);
            if(quoting) printf("%c",QUOTECHAR);
            printf("%s",EOL);
          }
        }
        poolClose(fd);
      }
      break;

    case WRITE: /* only for pool HV_KVP_POOL_GUEST */
      if(optind >= argc) errx(EX_USAGE,"Key and value must be defined in the command line");
      if((argc - optind) % 2 != 0) errx(EX_USAGE,"Key and value must be pairs");
      fd=poolOpen(HV_KVP_POOL_GUEST,O_RDWR,F_WRLCK);
      while(readKVPrecord(fd,&kvpDst,ctFromKVP,verbose)) { /* for each key */
        int findArgC=findKeyArgs(argc,argv,optind,2,kvpDst.key);
        if(findArgC) { /* overwrite value matched by key */
          if(lseek(fd,-sizeof(struct kvp_record),SEEK_CUR) == -1) err(EX_IOERR,"Cannot set file descriptor to the previous KVP record");
          writeKVPrecord(fd,argv[findArgC],argv[findArgC+1],ctToKVP,verbose);
          if(verbose) warnx("Key '%s' already exists",argv[findArgC]);
          argv[findArgC][0]=argv[findArgC+1][0]='\0'; /* pair: key, value - erased in argv */
        }
      }
      for(; optind<argc; optind+=2) { /* for each pair: key value */
        writeKVPrecord(fd,argv[optind],argv[optind+1],ctToKVP,verbose);
      }
      poolClose(fd);
      break;

    case REMOVE: /* only for pool HV_KVP_POOL_GUEST */
      if(optind >= argc) errx(EX_USAGE,"There must be at least one key to delete");
      fd=poolOpen(HV_KVP_POOL_GUEST,O_RDWR,F_WRLCK);
      off_t bufSize=lseek(fd,0,SEEK_END);
      if(bufSize == -1) err(EX_IOERR,"Cannot set file descriptor to end of file");
      if(bufSize % sizeof(struct kvp_record) != 0) errx(EX_DATAERR,"KVP records damaged");
      struct kvp_record *buf=malloc(bufSize);
      if(buf == NULL) err(EX_SOFTWARE,"Cannot allocate %ld bytes in the memory",bufSize);
      memset(buf,0,bufSize);
      if(lseek(fd,0,SEEK_SET) == -1) err(EX_IOERR,"Cannot set file descriptor to start of file");
      while(readKVPrecord(fd,&kvpDst,ctFromKVP,verbose)) { /* for each key */
        if(findKeyArgs(argc,argv,optind,1,kvpDst.key)) { /* matching by key */
          /* remove key from pool */
          off_t currKVPoffset=lseek(fd,-sizeof(struct kvp_record),SEEK_CUR);
          if(currKVPoffset == -1) err(EX_IOERR,NULL);
          ssize_t readSize=pread(fd,buf,bufSize,currKVPoffset+sizeof(struct kvp_record));
          if(readSize == -1) err(EX_IOERR,NULL);
          ssize_t writeSize=pwrite(fd,buf,readSize,currKVPoffset);
          if(writeSize == -1) err(EX_IOERR,NULL);
          if(writeSize != readSize) errx(EX_IOERR,"The read and written buffers are not equal");
          if(ftruncate(fd,currKVPoffset+writeSize) == -1) err(EX_IOERR,"Cannot truncate pool");
        }
      }
      free(buf);
      poolClose(fd);
      break;

    default: assert(0); /* unhandled commands */
  }
  return 0;
}
