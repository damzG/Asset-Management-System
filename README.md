# Asset Management System

A desktop-based **Asset Management System** developed in **C programming language** using the **Raylib** graphics library. The application is designed to provide a simple graphical interface for managing, viewing, adding, updating, and removing assets.

---

## Table of Contents

* [Project Overview](#project-overview)
* [Features](#features)
* [Technologies Used](#technologies-used)
* [Project Structure](#project-structure)
* [System Requirements](#system-requirements)
* [Installation](#installation)
* [Building the Project](#building-the-project)
* [Running the Application](#running-the-application)
* [How to Use](#how-to-use)
* [Asset Management](#asset-management)
* [Data Storage](#data-storage)
* [AI Tools Used During Development](#ai-tools-used-during-development)
* [Testing](#testing)
* [Known Issues](#known-issues)
* [Future Improvements](#future-improvements)
* [Contributors](#contributors)
* [License](#license)

---

## Project Overview

The **Asset Management System** is a C-based application developed to demonstrate the use of programming concepts and graphical user interfaces.

The system allows users to manage a collection of assets through a Raylib-based graphical interface.

The project was developed as part of:

**[Module / Course Name]**

The main objectives of the project are to:

* Develop a functional application using C.
* Use Raylib to create a graphical user interface.
* Implement data structures and file handling.
* Allow users to perform CRUD operations on assets.
* Apply programming concepts such as functions, structures, arrays, pointers, and file I/O.
* Practise debugging and software development techniques.
* Use AI development tools responsibly to assist with research, debugging, and development.

---

## Features

The system currently provides the following functionality:

### Asset Management

* Add a new asset.
* View existing assets.
* Search for assets.
* Update asset information.
* Delete assets.
* Categorise assets.
* Track asset status.
* Store asset information.

### Graphical User Interface

The application uses **Raylib** to provide a graphical interface.

The interface includes:

* Main menu.
* Asset list.
* Input forms.
* Buttons.
* Navigation controls.
* Status messages.
* Search functionality.

### Data Management

The system supports:

* Asset records.
* Asset categories.
* Asset identification numbers.
* Asset descriptions.
* Asset status.
* Asset ownership/location information.
* Persistent data storage.

> Update this section to match the exact features implemented in your project.

---

## Technologies Used

| Technology       | Purpose                                                       |
| ---------------- | ------------------------------------------------------------- |
| **C**            | Main programming language                                     |
| **Raylib**       | Graphics and GUI functionality                                |
| **[Compiler]**   | Compiling the C source code                                   |
| **[IDE/Editor]** | Development environment                                       |
| **Git/GitHub**   | Version control and project management                        |
| **AI Tools**     | Development assistance, debugging, research and documentation |

### AI Tools

AI tools used during development may include:

* **ChatGPT**
* **[Other AI Tool]**

AI was used as a development assistant rather than as a replacement for understanding or testing the code.

---

# Project Structure

An example project structure is shown below:

```text
AssetManagementSystem/
│
├── src/
│   ├── main.c
│   ├── assets.c
│   ├── assets.h
│   ├── ui.c
│   ├── ui.h
│   └── file_manager.c
│
├── include/
│   └── ...
│
├── data/
│   └── assets.txt
│
├── assets/
│   ├── images/
│   └── fonts/
│
├── README.md
└── Makefile
```

> Modify this structure to reflect the actual files and folders in your project.

---

# System Requirements

Before running the application, ensure that the following are installed:

* C compiler
* Raylib
* Git (optional)
* Compatible operating system

### Example

**Operating System:**

* Windows
* Linux
* macOS

**Compiler:**

* GCC / MinGW / Clang

**Raylib:**

* Version: `[Insert Version]`

---

# Installation

## 1. Clone the Repository

```bash
git clone [REPOSITORY_URL]
```

Navigate into the project directory:

```bash
cd AssetManagementSystem
```

---

## 2. Install Raylib

Install Raylib according to your operating system.

Official Raylib documentation:

https://www.raylib.com/

For Linux, Raylib can be installed using the appropriate package manager or built from source.

For Windows, Raylib can be configured using a compatible compiler such as MinGW.

---

# Building the Project

The project can be compiled using GCC.

Example:

```bash
gcc src/*.c -o AssetManagementSystem -lraylib
```

Additional libraries may be required depending on the operating system and Raylib configuration.

For example, on Linux:

```bash
gcc src/*.c -o AssetManagementSystem -lraylib -lm
```

> Replace these commands with the exact build command used by the project.

---

# Running the Application

After compiling the program, run the executable.

### Windows

```bash
AssetManagementSystem.exe
```

### Linux/macOS

```bash
./AssetManagementSystem
```

---

# How to Use

## Main Menu

When the application starts, the user is presented with the main menu.

The main menu provides access to the main system functions.

Example:

```text
--------------------------------
       ASSET MANAGEMENT SYSTEM
--------------------------------

1. View Assets
2. Add Asset
3. Search Assets
4. Update Asset
5. Delete Asset
6. Exit
```

> Replace this example with screenshots or descriptions of your actual interface.

---

## Adding an Asset

To add an asset:

1. Select **Add Asset**.
2. Enter the required asset information.
3. Enter the asset category.
4. Enter the asset status.
5. Confirm the information.
6. Save the asset.

Example asset:

```text
Asset ID:       A001
Name:           Dell Laptop
Category:       Computer Equipment
Location:       IT Department
Status:         Available
```

---

## Viewing Assets

The **View Assets** section displays the assets currently stored in the system.

Example:

| ID   | Name        | Category  | Location      | Status    |
| ---- | ----------- | --------- | ------------- | --------- |
| A001 | Dell Laptop | Computer  | IT Department | Available |
| A002 | HP Monitor  | Computer  | Office 1      | In Use    |
| A003 | Office Desk | Furniture | Office 2      | Available |

---

## Searching for an Asset

Users can search for assets using information such as:

* Asset ID
* Asset name
* Category
* Location
* Status

Example:

```text
Search: Dell
```

The system will display matching assets.

---

## Updating an Asset

Existing asset records can be modified when information changes.

For example:

```text
Status:
Available → In Use
```

or:

```text
Location:
Office 1 → Office 3
```

---

## Deleting an Asset

An asset can be removed from the system when it is no longer required.

The application should request confirmation before permanently deleting an asset.

Example:

```text
Are you sure you want to delete asset A001?

[Y] Yes
[N] No
```

---

# Data Storage

Asset information is stored using:

**[Text File / CSV / Binary File / Database]**

Example:

```text
A001,Dell Laptop,Computer,IT Department,Available
A002,HP Monitor,Computer,Office 1,In Use
A003,Office Desk,Furniture,Office 2,Available
```

The application loads the asset data when it starts and saves changes when assets are added, updated, or deleted.

> Replace this section with the actual storage method used by your project.

---

# AI Tools Used During Development

AI tools were used during the development of this project as **development assistance tools**.

The purpose of using AI was to support the development process while maintaining an understanding of the code and its implementation.

## Uses of AI

AI assistance was used for tasks such as:

### 1. Research

AI was used to explain programming concepts and provide guidance on:

* C programming.
* Raylib functionality.
* Structures.
* Arrays.
* Pointers.
* File handling.
* Functions.
* GUI programming.
* Debugging techniques.

### 2. Debugging

AI tools were used to help identify potential causes of compiler errors, runtime errors, and unexpected program behaviour.

Example:

```text
Problem:
The program crashes when deleting an asset.

AI assistance:
The issue was investigated by reviewing the relevant array
and memory handling code.
```

The suggested solution was then reviewed, tested, and modified where necessary.

### 3. Code Development

AI was used to provide examples and suggestions for implementing specific programming features.

Examples include:

* Raylib buttons.
* Input fields.
* Asset searching.
* File handling.
* Data structures.
* Menu navigation.

AI-generated suggestions were reviewed and adapted to suit the requirements of the project.

### 4. Documentation

AI was also used to assist with:

* README documentation.
* Code comments.
* Explaining programming concepts.
* Organising project documentation.

---

## Responsible Use of AI

AI was used as an **assistance tool** rather than as a replacement for the developer.

All AI-generated suggestions were:

1. Reviewed before being used.
2. Adapted to the requirements of the project.
3. Tested within the application.
4. Debugged when necessary.
5. Verified against the project's expected behaviour.

The developer remains responsible for the final implementation and understanding of the code.

---

# Example AI Prompt

An example prompt used during development:

```text
I am developing an Asset Management System in C using Raylib.

I need to create a function that searches an array of asset
structures by asset ID.

Explain how I could implement this and provide a simple C example.
```

Another example:

```text
I am getting a segmentation fault when accessing an asset
structure in my C program.

Here is my code:
[CODE]

Explain what could be causing the problem and suggest how
I can debug it.
```

---

# Testing

The application was tested to ensure that the main functionality operates correctly.

| Test          | Expected Result                    | Result      |
| ------------- | ---------------------------------- | ----------- |
| Add asset     | New asset is created               | [Pass/Fail] |
| View assets   | Assets are displayed               | [Pass/Fail] |
| Search asset  | Matching assets are displayed      | [Pass/Fail] |
| Update asset  | Asset information changes          | [Pass/Fail] |
| Delete asset  | Selected asset is removed          | [Pass/Fail] |
| Save data     | Asset data is stored correctly     | [Pass/Fail] |
| Load data     | Stored data is loaded correctly    | [Pass/Fail] |
| Invalid input | User receives an appropriate error | [Pass/Fail] |

---

# Known Issues

The following issues are currently known:

* [Issue 1]
* [Issue 2]
* [Issue 3]

If no known issues exist:

```text
No known issues at the current stage of development.
```

---

# Future Improvements

Potential future improvements include:

* User authentication.
* Role-based access control.
* Improved search and filtering.
* Asset borrowing/return tracking.
* Asset maintenance records.
* Improved graphical interface.
* Database integration.
* Exporting asset reports.
* Asset statistics and graphs.
* Barcode/QR code support.
* Automatic asset ID generation.
* Improved error handling.
* Multi-user support.

---

# Learning Outcomes

Through the development of this project, the following programming and software development concepts were practised:

* C programming.
* Structures.
* Arrays.
* Functions.
* Pointers.
* File handling.
* Input validation.
* Memory management.
* Event-driven programming.
* Graphical user interfaces.
* Debugging.
* Software testing.
* Version control.
* AI-assisted development.

---

# Contributors

**Developer:** Oyindamola Olaosun, Owen Nathanael

**Student Number:** C003131475, C00313648

**Course:** Advanced Programming

**Module:** Software Development

**Institution:** South East Technological Univerrsity

---

# License

This project was developed for **educational purposes**.

[Add your chosen license here, if applicable.]

---

# Acknowledgements

* **Raylib** — for providing the graphics and game development library used to create the application's graphical interface.
* **[College/University Name]** — for providing the learning environment and project requirements.
* **AI development tools** — for providing assistance with research, debugging, programming concepts, and documentation.
