# called by win32-x64.sh

# fail error code on uncaught exception
trap
{ 
  write-output $_ 
  exit 1 
} 

# read archive_name (archive does not exist yet)
$archiveName = [IO.File]::ReadAllText("archive_name").Trim()

# compress 'dist' folder to that archive
Add-Type -A System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory("build\\win32-x64\\dist", $archiveName)
