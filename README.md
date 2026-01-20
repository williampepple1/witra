# Witra - Wireless Transfer

**Send files and folders between computers on the same WiFi network.**

---

## Getting Started

### Installation

**Option 1: Installer**
- Run `Witra-Setup-1.0.0.exe`
- Follow the installation wizard
- Launch Witra from the Start Menu or Desktop

**Option 2: Portable**
- Extract `Witra-1.0.0-portable.zip` to any folder
- Run `witra.exe`

---

## How to Use

### 1. Connect to the Same WiFi

Both computers must be connected to the **same WiFi network**. Witra will automatically discover other devices running the app.

### 2. Find Devices in the Lobby

When you open Witra, you'll see the **Network** page (Lobby):

- Your profile name is shown at the top — you can change it
- Discovered devices appear as cards below
- Each card shows the device name and connection status

### 3. Connect to a Device

To send files, you first need to connect:

1. Click **Connect** on the device you want to send files to
2. The other person will see a connection request popup
3. They click **Accept** to allow the connection
4. Once connected, you'll see "Connected" status on both sides

### 4. Send Files

After connecting:

1. Click **Send Files** to select individual files, or
2. Click **Send Folder** to send an entire folder
3. Alternatively, **drag and drop** files directly onto the Witra window

### 5. Receive Files

When someone sends you files:

- A transfer automatically starts
- Switch to the **Transfers** tab to see progress
- Files are saved to `Downloads/Witra` in your home folder
- Click **Open Folder** when complete to view received files

---

## Transfers Page

The Transfers page shows all file transfers:

| Status | Meaning |
|--------|---------|
| **Transferring** | File is being sent/received |
| **Completed** | Transfer finished successfully |
| **Failed** | Something went wrong |
| **Cancelled** | Transfer was stopped |

- Click **Cancel** to stop an active transfer
- Click **Clear Completed** to remove finished transfers from the list

---

## Tips

- **Minimize to tray**: Closing the window minimizes Witra to the system tray. Double-click the tray icon to reopen.
- **Drag & drop**: Drop files anywhere on the window, then select which connected device to send to.
- **Large files**: Witra handles files of any size — transfers happen directly over your local network, not the internet.
- **Firewall**: If devices aren't appearing, check that your firewall allows Witra through (ports 45678 UDP and 45679 TCP).

---

## Troubleshooting

**Devices not appearing?**
- Ensure both computers are on the same WiFi network
- Check that Witra is allowed through Windows Firewall
- Try restarting the app

**Connection rejected?**
- The other person needs to click Accept on the connection request
- Make sure they have Witra open and visible

**Transfer failed?**
- Check that the receiving device has enough disk space
- Ensure the network connection is stable

---

## Download Location

All received files are saved to:
```
C:\Users\<YourName>\Downloads\Witra\
```

---

Made with ❤️ using C++ and Qt
