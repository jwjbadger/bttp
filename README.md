# BTTP

A simple HTTP server that hosts a folder written in C using standard syscalls.

## Why this project?

I wrote this with the intention of learning more about HTTP as a protocol as most clients or servers abstract away some of the nuances. While it may not serve an explicitly useful purpose, this project taught me about a number of features in C and served as an introduction to the language, its structure, and network programming. I was able to learn using this project:

- Memory management in C
    - Interacting with the heap using `malloc` and `free`
    - Debugging memory leaks
    - Working with data structures such as linked lists, etc.
- Very basic uses of Make
- Very basic network programming using `socket()`, `bind()`, `listen()`, etc.
- Working with files in C
    - Networking as an extension of files
    - Dynamically reading files by determining the length at runtime and allocating sufficient memory
- Error management and recovery
- Working with strings in C as pointers

## What can it do?

In short, it can't do very much. What it can do is open a server and process HTTP requests including their method, path, version, and headers. Based on this, it can respond with an error or the contents of a local file in the directory that is being served. The path is processed to prevent "../" from being used to read files outside of the path of the project. A response is then formulated using a struct that is provided and that is used to create a formatted response. 

## What problems did I have to solve?

- Turning the linked list of headers into a formatted string is surprisingly difficult as the size is hard to figure out
- Freeing the memory used by the linked list of headers without skipping the first or last element
- Not freeing memory that wasn't allocated on the heap
- Parsing invalid requests that are mostly valid (e.g. LF used instead of CRLF or no headers provided)
- Handling errors by responding to the request properly
- Parsing the request properly
- Handling multiple requests at once
- Not allowing the server to read outside the project directory

