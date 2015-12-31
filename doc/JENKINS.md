To set up Jenkins:

- (Mac) Install latest JDK
- Install Git
- Install Jenkins Native
- Manage Jenkins -> Configure System
  - Set up the E-mail Notification
  - (Windows) Set MSBUILD environment variable to path to MSBuild.Exe
    - C:\Windows\Microsoft.NET\Framework64\v?\MSBuild.exe
  - (Windows) Set VCREDIST_PATH environment variable to path to the MSVC redistributable folder
    - C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\redist\x64\Microsoft.VC140.CRT
  - (Mac) You may have to set PATH environment variable explicitly 
- Manage Jenkins -> Manage Plugins -> Available
  - Install "Git Plugin"
  - (Windows) Install "PowerShell plugin"

- Create new job, freestyle project
  - Source Code Management:
    - Select Git
    - point to repo
    - specify branches
    - Additional Behaviours: 
      - Enable advanced sub-modules behaviores
      - check "Recursively update submodules"
  - Build Triggers
    - Poll SCM
      - Specify time to poll it
  - Build:
    - Execute shell:
      - run the appropriate platform .sh file
      - on success, this creates a zipped distributable archive and places its path in "archive_name"
        in the root workspace directory
    - (Optional) Execute another step to upload "archive_name" to a central location
  - Post-build Actions:
    - E-mail Notification
