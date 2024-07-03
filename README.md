# File Transfer

This is the homework of Chapter 6 of Computer Network Experiment 2024 in CUMT.

This repository Implemented file transfer via UDP/TCP protocol, with security check and beautified output.

## Development

The compile environment is MinGW Clang Toolchian, posix thread, C++ 20 standard on Windows OS. Please make sure you have Windows Kits on your computer.

If you meet problems when compiling, give a try to [LLVM MinGW](https://github.com/mstorsjo/llvm-mingw/releases).

The program will save the received file to the `./received` by default, you can change the directory by using the `--dir` option.

For more information, please refer to the help message via `--help`.

```shell
Usage: transf_server [ip] <port> [options]

Options:
  -h, --help               Display this help message
  --debug                  Enable debug mode
  -d, --dir <path>         Save received files to the specified directory
  --protocol <protocol>    Specify the protocol to use (default: udp)
  --tcp                    Equivalent to --protocol tcp
  --udp                    Equivalent to --protocol udp
  --chunk <size>           Set chunk size for file transfer (default: 2048)
  --timeout <timeout>      Set timeout for sending and receiving data (default: 10000)

Copyright (c) 2024 Jevon Wang, MIT License
Source code: github.com/cnily03-hive/transf
```

```shell
Usage: transf_client <ip> <port> [options]

Options:
  -h, --help               Display this help message
  --debug                  Enable debug mode
  --ping                   Ping the server
  --protocol <protocol>    Specify the protocol to use (default: udp)
  --tcp                    Equivalent to --protocol tcp
  --udp                    Equivalent to --protocol udp
  --timeout <timeout>      Set timeout for sending and receiving data (default: 10000)

Copyright (c) 2024 Jevon Wang, MIT License
Source code: github.com/cnily03-hive/transf
```

> [!TIP]
> The source code doesn't examine the lock of the file system, and it will overwrite the file if the file exists. Please be careful when using the program, though it have been equipped with some security check and mutex control.

This program will be updated in the future, welcome to contribute.

## License

Copyright (c) Cnily03. All rights reserved.

Licensed under the [MIT](./LICENSE) License.
