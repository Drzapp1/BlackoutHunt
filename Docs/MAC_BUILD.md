# Mac Build

Blackout Hunt cannot be packaged for macOS from the current Lenovo Windows machine. The supported path without owning Apple hardware is to rent an Apple-hardware build host, package there, and copy the unsigned tester artifact back to `D:\BlackoutHunt\Builds\Mac`.

Use AWS EC2 Mac by default. Mac instances run on Apple hardware, support SSH/ARD access, and have a 24-hour minimum Dedicated Host allocation. Epic's UE 5.7 macOS requirements are macOS Sonoma 14.0 minimum with Xcode 15.2 minimum and Xcode 15.4 or newer recommended; this project requires Xcode 15.4 or newer for the scripted build.

References:

- Apple macOS Software License Agreement: `https://www.apple.com/legal/sla/`
- Epic macOS development requirements: `https://dev.epicgames.com/documentation/unreal-engine/macos-development-requirements-for-unreal-engine`
- AWS EC2 Mac instances: `https://docs.aws.amazon.com/AWSEC2/latest/UserGuide/ec2-mac-instances.html`

## Local D-Drive Preparation

Do not install AWS, Epic, or Apple tooling on the Windows machine for this flow. Keep local files under `D:\BlackoutHunt`.

From PowerShell:

```powershell
cd D:\BlackoutHunt
.\Tools\Build-Editor.ps1
.\Tools\New-MacBuildInput.ps1
```

The transfer input is written to:

```text
D:\BlackoutHunt\Builds\MacTransfer
```

Use `-ManifestOnly` when you only want to audit the file list before creating the large zip:

```powershell
.\Tools\New-MacBuildInput.ps1 -ManifestOnly
```

The manifest must include project source paths such as `BlackoutHunt.uproject`, `Config`, `Content`, `Plugins`, `Source`, and `Tools`, while excluding generated or package-heavy directories named `.git`, `Binaries`, `Builds`, `DerivedDataCache`, `Intermediate`, and `Saved`.

## AWS Host Setup

Use the AWS Console or AWS CloudShell so no AWS credentials are written to the Windows machine.

1. Create a daily AWS Budget before allocating the host. Set a low alert threshold for the expected one-day Mac build spend.
2. Confirm EC2 quota for `mac2-m2pro.metal` Dedicated Hosts in `us-east-1`. If capacity or quota blocks the build, use `us-west-2`.
3. Allocate one Dedicated Host for `mac2-m2pro.metal`. Keep the selected Availability Zone for the launch step.
4. Launch a macOS Sonoma EC2 Mac AMI onto that Dedicated Host.
5. Configure root storage as encrypted `gp3`, 500 GB, 10,000 IOPS, and 400 MiB/s throughput.
6. Create a security group that allows SSH TCP `22` only from your current public IP.
7. Create or select an EC2 key pair. Store the downloaded `.pem` under:

```text
D:\BlackoutHunt\Builds\MacTransfer
```

8. Wait for EC2 instance checks to pass. AWS says Mac instance readiness can take several minutes.

Connect from Windows:

```powershell
ssh -i D:\BlackoutHunt\Builds\MacTransfer\blackouthunt-mac.pem ec2-user@<public-dns-or-ip>
```

## Remote Mac Configuration

On the Mac, install or select Xcode 15.4 or newer and accept the license:

```sh
sudo xcode-select -s /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -license accept
xcodebuild -version
```

Install Unreal Engine 5.7 for macOS through Epic Games Launcher on the remote Mac. Use ARD or another approved GUI connection if Epic login is required. The expected engine path is:

```text
/Users/Shared/Epic Games/UE_5.7
```

Verify Unreal AutomationTool:

```sh
UE_ROOT="/Users/Shared/Epic Games/UE_5.7"
"$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh" -help >/dev/null
```

## Transfer And Package

From Windows, upload the newest transfer zip:

```powershell
$zip = Get-ChildItem D:\BlackoutHunt\Builds\MacTransfer\BlackoutHunt-*-MacBuildInput-*.zip | Sort-Object LastWriteTime -Descending | Select-Object -First 1
scp -i D:\BlackoutHunt\Builds\MacTransfer\blackouthunt-mac.pem $zip.FullName ec2-user@<public-dns-or-ip>:/Users/ec2-user/BlackoutHunt-MacBuildInput.zip
```

On the Mac:

```sh
rm -rf /Users/ec2-user/BlackoutHunt
mkdir -p /Users/ec2-user/BlackoutHunt
ditto -x -k /Users/ec2-user/BlackoutHunt-MacBuildInput.zip /Users/ec2-user/BlackoutHunt
cd /Users/ec2-user/BlackoutHunt
UE_ROOT="/Users/Shared/Epic Games/UE_5.7" bash Tools/Package-Mac.sh Shipping
```

The package script writes:

```text
Builds/Mac/BlackoutHunt.app
Builds/Mac/BlackoutHunt-Mac-Shipping-<timestamp>.zip
Builds/Mac/BlackoutHunt-Mac-Shipping-<timestamp>.zip.sha256
Builds/Mac/Logs/package-mac-Shipping-<timestamp>.log
```

If UAT archives the app one level deeper, the script still finds `BlackoutHunt.app`, zips it, and writes the zip and checksum in `Builds/Mac`.

## Remote Verification

On the Mac:

```sh
cd /Users/ec2-user/BlackoutHunt
APP="$(find Builds/Mac -maxdepth 5 -type d -name 'BlackoutHunt.app' | head -n 1)"
test -n "$APP"
test -x "$APP/Contents/MacOS/BlackoutHunt"
"$APP/Contents/MacOS/BlackoutHunt" -nullrhi -unattended -nosound -nop4 -log
codesign --verify --deep --strict --verbose=2 "$APP" || true
```

The first macOS distro is expected to be unsigned or ad-hoc signed. Notarization is a later release step that requires an Apple Developer Program identity and signing assets.

Copy artifacts back to Windows:

```powershell
New-Item -ItemType Directory -Force -Path D:\BlackoutHunt\Builds\Mac | Out-Null
scp -i D:\BlackoutHunt\Builds\MacTransfer\blackouthunt-mac.pem "ec2-user@<public-dns-or-ip>:/Users/ec2-user/BlackoutHunt/Builds/Mac/*.zip*" D:\BlackoutHunt\Builds\Mac\
scp -i D:\BlackoutHunt\Builds\MacTransfer\blackouthunt-mac.pem "ec2-user@<public-dns-or-ip>:/Users/ec2-user/BlackoutHunt/Builds/Mac/Logs/*.log" D:\BlackoutHunt\Builds\Mac\
```

Confirm locally:

```powershell
Get-ChildItem D:\BlackoutHunt\Builds\Mac\BlackoutHunt-Mac-Shipping-*.zip*
Get-FileHash D:\BlackoutHunt\Builds\Mac\BlackoutHunt-Mac-Shipping-*.zip -Algorithm SHA256
```

## Teardown

After copying the artifacts back:

1. Stop or terminate the EC2 Mac instance.
2. Wait for AWS host scrubbing to complete.
3. Release the Dedicated Host after the 24-hour minimum allocation period.
4. Delete temporary security groups and key pairs if they are no longer needed.
5. Keep an AMI snapshot only if the Xcode and UE install worked and you plan to build again.

