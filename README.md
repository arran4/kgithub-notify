# Kgithub-notify

A GitHub KDE task program written in C++ that notifies you when there is a new GitHub notification.

When you click on the notification, it presents you with its own version of:
*   [GitHub Notifications](https://github.com/notifications)
*   [GitHub Pull Requests](https://github.com/pulls)
*   The GitHub feed/wall
*   Explore

## Build Instructions

### Prerequisites

*   C++ Compiler (C++17 support required)
*   CMake (version 3.10 or higher)

### Building

1.  Clone the repository:
    ```bash
    git clone https://github.com/yourusername/Kgithub-notify.git
    cd Kgithub-notify
    ```

2.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```

3.  Configure and build the project:
    ```bash
    cmake ..
    cmake --build .
    ```

4.  Run the application:
    ```bash
    ./Kgithub-notify
    ```

## License

This project is licensed under the BSD 3-Clause License - see the [LICENSE](LICENSE) file for details.
