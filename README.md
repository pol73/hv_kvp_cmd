<!-- @format -->

# hv_kvp_cmd

Hyper-V KVP user command

_This project allows you to work with KVP Hyper-V data from the virtual machine and host side._

[Hyper-V Data Exchange Service (KVP)](https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-data-exchange-service-kvp)

## I love one-liners code ;) ...

#### This is for reading the KVP of a virtual machine named "VM_name" and internal pools:

```powershell
(Get-CimInstance -Namespace root/virtualization/v2 -ClassName Msvm_ComputerSystem -Filter 'ElementName="VM_name"' | Get-CimAssociatedInstance -ResultClassName Msvm_KvpExchangeComponent).GuestIntrinsicExchangeItems | %{$Item=([XML]$_).SelectSingleNode("/INSTANCE/PROPERTY[@NAME='Name']/VALUE/child::text()"); if($Item -ne $null) {'"{0}"="{1}"' -f $Item.Value,$Item.SelectSingleNode("/INSTANCE/PROPERTY[@NAME='Data']/VALUE/child::text()").Value}}
```

#### This is for reading the KVP of a virtual machine named "VM_name" and guest pool:

```powershell
(Get-CimInstance -Namespace root/virtualization/v2 -ClassName Msvm_ComputerSystem -Filter 'ElementName="VM_name"' | Get-CimAssociatedInstance -ResultClassName Msvm_KvpExchangeComponent).GuestExchangeItems | %{$Item=([XML]$_).SelectSingleNode("/INSTANCE/PROPERTY[@NAME='Name']/VALUE/child::text()"); if($Item -ne $null) {'"{0}"="{1}"' -f $Item.Value,$Item.SelectSingleNode("/INSTANCE/PROPERTY[@NAME='Data']/VALUE/child::text()").Value}}
```
