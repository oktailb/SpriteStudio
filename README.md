# 🌟 Sprite Studio (WIP)

## Animated Sprite Management and Assembly Tool

**Sprite Studio** is a work-in-progress (WIP) development tool designed to help game creators and pixel artists efficiently manage, slice, and arrange animation sequences from various source materials (GIFs, consolidated image files, or individual frames).

![A quick demo of the Sprite Studio interface showing frame extraction, list management, and animation preview.](SpriteStudio.gif)

---

## ✨ Key Features

### Import & Data Handling
* **Flexible Import:** Supports importing **Animated GIFs** (automatic frame extraction) and **Sprite Sheets** (automatic slicing based on alpha/color tolerance).
* **Visual Arrangement:** Frames are managed in a list with intuitive drag-and-drop support for reordering and insertion.

### Editing & Sequencing
* **Frame Merging:** Easily merge frames by dropping one item onto another to combine them into a single frame.
* **Batch Operations (Multi-Selection):**
    * **Group Deletion** for selected frames.
    * **Invert Selection** to quickly select all unselected frames.
    * **Reverse Order** for selected frames to create quick ping-pong animations or correct sequencing issues.

### Animation Preview
* **Real-time Preview:** Animate the **currently selected frames** in sequence, respecting the user-defined **FPS** (Frames Per Second).
* **Fixed Aspect Ratio:** Ensures small frames are centered within the bounding box of the largest frame, preventing perceived size changes during playback.

### Export
* **Industry-Standard Export:** Generates the final **Sprite Atlas** as a **PNG** image.
* **Metadata Export:** Creates a **JSON** metadata file (compatible with tools like Texture Packer) containing the exact coordinates (x, y, w, h) of each frame on the exported atlas.

---

## 💻 Building and Running

### Prerequisites

* A C++ compiler.
* **CMake** (version 3.10 or higher recommended).
* The necessary development dependencies (e.g., Qt framework libraries, as specified in `CMakeLists.txt`).

### Build Steps

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/oktailb/SpriteStudio
    cd SpriteStudio.git
    ```

2.  **Configure and Compile (using CMake):**
    ```bash
    mkdir build
    cd build
    # Configure the project, assuming CMakeLists.txt is in the root
    cmake ..
    # Build the project
    cmake --build . 
    # Or simply: make
    ```

3.  **Launch the application:**
    ```bash
    ./SpriteStudio
    ```

---

## 🗺️ Roadmap (Future Plans)

* **Project Save/Load:** Implement a dedicated project file format (`.sps`) to save the current frame order, settings, and source data, plans also to support `.tps` format.
* **Manual Bounding Box Editing:** Allow users to manually adjust frame boundaries for precise slicing.
* **Export Formats:** Add specific metadata formats (e.g., XML, formats tailored for popular game engines).

---

## ✍️ Development

**Developer:** Vincent LECOQ

## 📄 License

This project is licensed under the **Apache License 2.0**. See the `LICENSE` file for full details.
