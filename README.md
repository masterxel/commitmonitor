
# CommitMonitor

CommitMonitor is a small tool to monitor Subversion and Git repositories for new commits. It has a very small memory footprint and resides in the system tray.

## Main UI

The new commits are shown on the top right of the main dialog, while the commit log message is shown at the bottom right.

A double click on any revision in the top right view will fetch the diff for that revision as a unified diff so you can further inspect the commit. If you have TortoiseSVN/TortoiseGit installed, CommitMonitor automatically uses TortoiseSVN/TortoiseGit to do the diff.

## Notifications

Once CommitMonitor has found new commits to one or more of the repositories you monitor, it shows a notification popup, and the system tray icon changes the "eyes" from black to red. And if you have the system tray animation enabled in the Options dialog, the eyes will also move around until you open CommitMonitor's main dialog (by doubleclicking on the system tray icon) and read the commits.

## Resource Usage

As already mentioned, CommitMonitor resides in your system tray (if so required). Tools which do that should use as less resources as possible, and that's what CommitMonitor tries to do. As you can see in the screenshot below, when the program is idle (i.e., not currently connecting to a repository and downloading information) it only uses about 1MB of RAM. Of course, it uses more (about 15MB RAM) while it accesses the repositories.

## License and Download

CommitMonitor is available under the GNU GPL v2.
You can either download an msi installer or a zipped exe file, whatever suits you. Download the latest version from the Releases link on github.

## Author

CommitMonitor was written by Stefan Kueng and this fork has added Git repository support.
