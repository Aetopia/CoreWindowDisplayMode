# CoreWindowDisplayMode
Run any UWP app that uses a [core window](https://learn.microsoft.com/en-us/uwp/api/windows.ui.core.corewindow) to run at an arbitrary display mode when in fullscreen.

## Usage
1. Open PowerShell and run the following:<br>

    ```powershell
    $AppxPackages = Get-AppxPackage
    Get-StartApps | 
    ForEach-Object { 
        $StartApp = $_ 
        $AppxPackage = $AppxPackages | Where-Object { $StartApp.AppID -like "$($_.PackageFamilyName)*" }
        if ($AppxPackage) { Write-Host "$($StartApp.Name): $($StartApp.AppID)" } 
    }
    ```

    This will list all UWP apps present within the Windows Start Menu.

    Output:<br>
    ```
    Settings: windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel
    ```

2. Copy the App User Model ID for an UWP app.

3. Download the latest release from [GitHub Releases](https://github.com/Aetopia/CoreWindowDisplayMode/releases/latest).

4. Open Command Prompt or PowerShell and pass the arguments as follows:<br>

    - Syntax: 
        ```cmd
        CoreWindowDisplayMode.exe <App User Model ID> <Width> <Height> <Refresh Rate>
        ```
    
    - Example:
        ```cmd
        CoreWindowDisplayMode.exe windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel 1024 768 60
        ```

    This tells the program to do the following:<br>
    1. Launch the specified UWP app.
    2. Use the specified width, height and refresh rate for the desired display mode.
        > The program only accepts landscape based resolutions.
        
    3. Hook onto the UWP app's window to listen to window events.
    
    The program will automatically exit when the UWP app exits.


The desired display mode will be only applyed if:
- The UWP app in is fullscreen.
- The UWP app is the current foreground window.

If any of the above conditions are false, the display mode will be reverted.


## Building
1. Install [MSYS2](https://www.msys2.org/) & [UPX](https://upx.github.io/) for optional compression.
2. Update the MSYS2 Environment until there are no pending updates using:

    ```bash
    pacman -Syu --noconfirm
    ```

3. Install GCC & C++/WinRT using:

    ```bash
    pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cppwinrt --noconfirm
    ```

3. Make sure `<MSYS2 Installation Directory>\ucrt64\bin` is added to the Windows `PATH` environment variable.
4. Run [`Build.cmd`](Build.cmd).
    