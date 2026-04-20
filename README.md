# 🖥️ WindowNest – Window Management Utility

A C++ Win32 application to capture, embed, and control external windows with transparency and overlay features.

---

## ✨ Features

* Capture and embed any running window
* Adjustable opacity (10% – 100%)
* Always-on-top overlay mode
* Custom UI built using Win32 API & GDI
* Keyboard shortcuts for quick control

---

## 🚀 Run the App (Easy Way)

1. Go to the **Releases** section
2. Download `WindowHider.exe`
3. Double-click to run

---

## 🧑‍💻 Build from Source (Optional)

### 🔧 Requirements

* Windows OS
* MinGW / g++ compiler

### ▶️ Compile

```
g++ WindowHider.cpp -o WindowHider.exe -mwindows -lcomctl32 -lgdi32 -luser32
```

### ▶️ Run

```
WindowHider.exe
```

---

## ⚙️ How to Use

1. Click **Pick Window**
2. Hover over any window to capture it
3. Adjust opacity using the slider
4. Use overlay mode for multitasking
5. Click **Release** to restore

---

## ⌨️ Shortcuts

* Ctrl + Shift + B → Toggle overlay mode
* Ctrl + Shift + H → Hide / Show app

---

## 🛠️ Tech Stack

* C++
* Win32 API
* GDI

---

## ⚠️ Disclaimer

This project is developed for educational purposes only.
