# FSBUS

**Version:** 3.5.0  
**Authors:** Dirk Anderseck & Francesco Talotta  
**Repository:** [github.com/FrancescoTalotta/FSBUS](https://github.com/FrancescoTalotta/FSBUS)

## 🚀 New in Version 3.5.0

This release includes several key improvements:

- ✅ Fixed a **serial port COM bug**
- 🆕 Added **16-bit Analog Input**
- 🆕 Added **16-bit Analog Output**

---

## 🧰 Build Instructions (Windows)

The project is built using **Visual Studio Code** with the **C++ development environment** and **CMake support**.

### ✅ Requirements

- [Visual Studio Code](https://code.visualstudio.com/)
- [CMake Tools extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
- Visual Studio 2022 with **Desktop development with C++** workload
- Git


---

## ⚙️ Post-Build File Copy

To automatically copy the DLL, LIB, and header files into another project, you should adapt the paths in the `CMakeLists.txt`:

```
# Add post-build commands to copy the files 
add_custom_command( 
    TARGET fsbus 
    POST_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy 
            $<TARGET_FILE:fsbus> 
            C:/Users/your-username/Documents/PROGRAMS/GA_SIM/build/Release 

    COMMAND ${CMAKE_COMMAND} -E copy 
            ${CMAKE_SOURCE_DIR}/fsbus.h 
            C:/Users/your-username/Documents/PROGRAMS/GA_SIM 

    COMMAND ${CMAKE_COMMAND} -E copy 
            ${CMAKE_BINARY_DIR}/Release/fsbus.lib 
            C:/Users/your-username/Documents/PROGRAMS/GA_SIM 
)
```

## 💙 Support the Project

If you find this project useful, consider supporting it with a small donation.  
Your contribution helps cover development time, testing equipment, and future improvements.

[![Donate](https://img.shields.io/badge/Donate-PayPal-blue.svg)](https://www.paypal.com/donate/?hosted_button_id=3K2V8JSVAMUHJ)
