# Wrapper: run a Node.js script using the DSH Electron binary as the Node runtime.
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$NodeArgs
)
$env:ELECTRON_RUN_AS_NODE = '1'
$exe = 'C:\Users\Shiyu\AppData\Local\Programs\DSH Desktop\DSH Desktop.exe'
& $exe $Script @NodeArgs
exit $LASTEXITCODE
