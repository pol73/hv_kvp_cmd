<# 
.SYNOPSIS
Get all KVP object from all running Hyper-V VM
.DESCRIPTION
Get all KVPs belonging to all virtual machines passed through a parameter or input pipe.
.INPUTS
Microsoft.HyperV.PowerShell.VirtualMachine
.COMPONENT
Hyper-V
.ROLE
Hyper-V Administrators security group
.EXAMPLE
Get-VM | Get-KVP
.LINK
https://github.com/pol73/hv_kvp_cmd
.LINK
https://docs.microsoft.com/en-us/virtualization/hyper-v-on-windows/reference/integration-services#hyper-v-data-exchange-service-kvp
#>
[CmdletBinding()]
[Alias('gkvp')]
[OutputType([PSCustomObject])]
Param([Parameter(Mandatory, ValueFromPipeline, Position = 0, HelpMessage = 'This is a Hyper-V virtual machyne object.')][Microsoft.HyperV.PowerShell.VirtualMachine]$VM)
begin {
  function New-KVP {
    param ([Parameter(Mandatory, ValueFromPipeline, HelpMessage = 'XML with all KVP')][XML]$in)
    begin { $out = [PSCustomObject]@{ PSTypeName = 'Hv.KVP.strings' } }
    process {
      if ($null -ne $in) {
        $item = $in.SelectSingleNode("/INSTANCE/PROPERTY[@NAME='Name']/VALUE/child::text()")
        if ($null -ne $item) {
          $key = $item.Value
          if ($null -ne $key) {
            $value = $item.SelectSingleNode("/INSTANCE/PROPERTY[@NAME='Data']/VALUE/child::text()").Value
            [string]$skey = $key.ToString()
            [string]$svalue = $null -eq $value ? '' : $value.ToString()
            $out | Add-Member -MemberType NoteProperty -Name $skey -Value $svalue
          }
          else { Write-Warning -Message 'The key cannot be empty.' }
        }
      }
    }
    end { $out }
  }
}
process {
  if ($VM.State -match 'Running') {
    $vmid = $VM.VMId.ToString()
    $cim = Get-CimInstance -Namespace root/virtualization/v2 -ClassName Msvm_ComputerSystem -Filter ("Name='{0}'" -f $vmid.ToUpper()) -Property Name | Get-CimAssociatedInstance -ResultClassName Msvm_KvpExchangeComponent
    $item = [PSCustomObject]@{
      PSTypeName = 'Hv.KVP'
      VMId       = $vmid
      VMName     = $null -eq $VM.VMName ? '' : $VM.VMName.ToString()
      Intrinsic  = $cim.GuestIntrinsicExchangeItems | New-KVP
      Guest      = $cim.GuestExchangeItems | New-KVP
    }
    $item
  }
}
end {}