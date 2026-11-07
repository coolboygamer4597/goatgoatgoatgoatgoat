param([Parameter(Mandatory=$true)][string]$SourceFile,[Parameter(Mandatory=$true)][string]$OutputFile)
$ErrorActionPreference = 'Stop'
$sourcePath = [IO.Path]::GetFullPath($SourceFile)
$outputPath = [IO.Path]::GetFullPath($OutputFile)
$textSource = [IO.File]::ReadAllText($sourcePath).Replace("`r`n", "`n")
$parts = [regex]::Matches($textSource, 'R"\((.*?)\)"', [Text.RegularExpressions.RegexOptions]::Singleline)
if ($parts.Count -eq 0) { throw 'No native sky shader source found.' }
$shaderSource = [string]::Concat([string[]]@($parts | ForEach-Object { $_.Groups[1].Value }))
$sourceBytes = [Text.Encoding]::UTF8.GetBytes($shaderSource)
$sha = [Security.Cryptography.SHA256]::Create()
try { $sourceHash = [BitConverter]::ToString($sha.ComputeHash($sourceBytes)).Replace('-', '').ToLowerInvariant() } finally { $sha.Dispose() }
$nativePath = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetDirectoryName($sourcePath)) '../native/NativeChams.cpp'))
$projectionPath = [IO.Path]::GetFullPath((Join-Path ([IO.Path]::GetDirectoryName($sourcePath)) '../../app/TextureProjectionShader.h'))
$nativeText = [IO.File]::ReadAllText($nativePath).Replace("`r`n", "`n")
$projectionText = [IO.File]::ReadAllText($projectionPath).Replace("`r`n", "`n")
$nativeParts = [regex]::Matches($nativeText, 'R"HLSL\((.*?)\)HLSL"', [Text.RegularExpressions.RegexOptions]::Singleline)
$projectionParts = [regex]::Matches($projectionText, 'R"HLSL\((.*?)\)HLSL"', [Text.RegularExpressions.RegexOptions]::Singleline)
if ($nativeParts.Count -lt 3 -or $projectionParts.Count -lt 1) { throw 'Native material source is incomplete.' }
$nativeSource = $nativeParts[0].Groups[1].Value + $projectionParts[0].Groups[1].Value
for ($partIndex=1; $partIndex -lt $nativeParts.Count; ++$partIndex) { $nativeSource += $nativeParts[$partIndex].Groups[1].Value }
$nativeBytes = [Text.Encoding]::UTF8.GetBytes($nativeSource)
$nativeSha = [Security.Cryptography.SHA256]::Create()
try { $nativeHash = [BitConverter]::ToString($nativeSha.ComputeHash($nativeBytes)).Replace('-', '').ToLowerInvariant() } finally { $nativeSha.Dispose() }
if ([IO.File]::Exists($outputPath)) {
    $existing = [IO.File]::ReadAllText($outputPath)
    if ($existing.Contains('SourceHash[]="' + $sourceHash + '"') -and $existing.Contains('GeneratorRevision=2;') -and $existing.Contains('NativeSourceHash[]="' + $nativeHash + '"')) { Write-Output 'Native sky programs are current.'; exit 0 }
}
$env:LIB = [string]::Join(';', [string[]]@($env:LIB.Split(';') | Where-Object { $_ -and [IO.Directory]::Exists($_) }))
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace NativeSkyBuild {
 [ComImport, Guid("8BA5FB08-5195-40E2-AC58-0D989C3A0102"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
 public interface Blob {
  [PreserveSig] IntPtr GetBufferPointer();
  [PreserveSig] UIntPtr GetBufferSize();
 }
 public static class Compiler {
  [DllImport("d3dcompiler_47.dll", CallingConvention=CallingConvention.StdCall)]
  static extern int D3DCompile(byte[] source, UIntPtr length, IntPtr name, IntPtr macros, IntPtr include,
   [MarshalAs(UnmanagedType.LPStr)] string entry, [MarshalAs(UnmanagedType.LPStr)] string profile,
   uint flags, uint flags2, out Blob output, out Blob errors);
  public static byte[] Compile(byte[] source, string entry, string profile) {
   Blob output=null, errors=null;
   try {
    int result=D3DCompile(source,(UIntPtr)source.Length,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,entry,profile,32768,0,out output,out errors);
    if(result<0) throw new InvalidOperationException(errors==null ? "Sky shader compilation failed." : Marshal.PtrToStringAnsi(errors.GetBufferPointer(),checked((int)errors.GetBufferSize().ToUInt64())));
    byte[] bytes=new byte[checked((int)output.GetBufferSize().ToUInt64())];Marshal.Copy(output.GetBufferPointer(),bytes,0,bytes.Length);return bytes;
   } finally { if(errors!=null) Marshal.ReleaseComObject(errors);if(output!=null) Marshal.ReleaseComObject(output); }
  }
 }
}
'@
$entries = @('vs','sky','glow','skyMS','maskVisible','maskAll','outline','fog','fogMS','weather','reflection','reflectionComposite','visibility','resolveNativeDepth','waterEnvironment','weatherVS')
$builder = [Text.StringBuilder]::new()
[void]$builder.Append("#pragma once`n#include <cstddef>`nnamespace GameSkyPrograms {`ninline constexpr int GeneratorRevision=2;`ninline constexpr char SourceHash[]=`"$sourceHash`";`n")
for ($entryIndex=0; $entryIndex -lt $entries.Count; ++$entryIndex) {
    $shaderProfile = if ($entryIndex -eq 0 -or $entries[$entryIndex] -eq 'weatherVS') { 'vs_5_0' } else { 'ps_5_0' }
    $bytes = [NativeSkyBuild.Compiler]::Compile($sourceBytes,$entries[$entryIndex],$shaderProfile)
    [void]$builder.Append("inline constexpr unsigned char Code$entryIndex[]={`n")
    for ($offset=0; $offset -lt $bytes.Length; $offset+=32) {
        $count = [Math]::Min(32,$bytes.Length-$offset)
        [void]$builder.Append('0x').Append([BitConverter]::ToString($bytes,$offset,$count).Replace('-',',0x')).Append(",`n")
    }
    [void]$builder.Append("};`n")
    Write-Output ("Prepared native sky program {0}: {1} bytes" -f $entries[$entryIndex],$bytes.Length)
}
[void]$builder.Append("struct Program { const unsigned char* data; std::size_t size; };`ninline constexpr Program All[]={`n")
for ($entryIndex=0; $entryIndex -lt $entries.Count; ++$entryIndex) { [void]$builder.Append("{Code$entryIndex,sizeof(Code$entryIndex)},`n") }
[void]$builder.Append("};`ninline constexpr char NativeSourceHash[]=`"$nativeHash`";`n")
$materialBytes = [NativeSkyBuild.Compiler]::Compile($nativeBytes,'ps_main','ps_5_0')
[void]$builder.Append("inline constexpr unsigned char NativeMaterial[]={`n")
for ($offset=0; $offset -lt $materialBytes.Length; $offset+=32) {
    $count = [Math]::Min(32,$materialBytes.Length-$offset)
    [void]$builder.Append('0x').Append([BitConverter]::ToString($materialBytes,$offset,$count).Replace('-',',0x')).Append(",`n")
}
[void]$builder.Append("};`n}`n")
Write-Output ("Prepared native material program: {0} bytes" -f $materialBytes.Length)
[void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($outputPath))
[IO.File]::WriteAllText($outputPath,$builder.ToString(),[Text.UTF8Encoding]::new($false))
