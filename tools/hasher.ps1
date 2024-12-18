function compute_crc16 {
    Param($data)

    $size = $data.Length
    $i = 0
    $crc = 0
    while ($size--)
    {
        $crc = $crc -bxor $data[$i++]
        for ($k = 0; $k -lt 8; $k++) {
            $crc = if ($crc -band 1) { ($crc -shr 1) -bxor 0xa001 } else { $crc -shr 1 }
        }
    }
    return $crc
}

$message = $args[0]

$hash = compute_crc16([System.Text.Encoding]::UTF8.GetBytes($message.Trim('"').Replace("\`"", "`"").Replace("\\", "\").Replace("\n", "`n")))
Write-Output($hash)