README
======

VZIP Project
============

Overview
--------
This project involves speeding up a video compression tool that processes a folder of uncompressed image files (".ppm" extension) and creates a single zip file with all images after compression. The images in the input folder are named with their frame numbers, and the output zip file should follow lexicographical order.

This is a group project with strict requirements on group size and threading limits. The code must be written in C using the pthread library and tested in a specified Linux environment.

Group Requirements
------------------
- Minimum group size: 3 members
- Maximum group size: 3 members
- No individual work allowed
- No groups of two or more than three members allowed

Task Instructions
-----------------
1. Download the starting package from the provided link.
2. Unzip, build, and run the tool. You might need to install the zlib library.
    ```
    $ unzip project2.zip
    $ cd project2
    $ make
    $ make test
    ```
3. Modify the code to make it faster using threads, with the following constraints:
    - Only use the pthread library.
    - No more than 20 threads (including the main thread) can be running simultaneously.

Submission Requirements
------------------------
The submission is divided into two parts:

PART #1:
- Create a file named `vzip.zip` containing a folder named `src`.
- The `src` folder must contain:
    - Your source code in C.
    - A Makefile to build the executable named `vzip`.
- Ensure your code is buildable and runnable in a Linux environment with gcc version 11.4.0.

Achieving a speedup three times faster than the original code guarantees a C for the speeding factor criterion. The fastest group in class will define the A+ grade (full credit).

Code style will have deductions for bad indentation, organization, and lack of meaningful comments.

Extra Credit Opportunity (20 points)
-------------------------------------
Implement a visualizer for vzip videos for up to 20 points of extra credit. Show your visualizer working during your video presentation and submit its code to Canvas.

Visualizer Task Description:
1. Implement a loader for a vzip file that decompresses its frames.
2. Show each frame on screen, one after the other, as if playing a video.

Visualizer Important Remarks:
1. No need to implement control functions (play, stop, rewind, etc.).
2. The visualizer can use any library (but must use zlib to decompress the video file).
3. You don't have to use C language for the visualizer.
4. No need to use threads in the visualizer.
5. Include a README file with instructions to build and run your code in your submission.

Project Setup
-------------
1. Ensure you have zlib installed:
    ```
    sudo apt-get install zlib1g-dev
    ```
2. Unzip and build the project:
    ```
    $ unzip project2.zip
    $ cd project2
    $ make
    ```
3. Run the tests:
    ```
    $ make test
    ```

Threading Implementation
-------------------------
- Use the pthread library to implement threading.
- Ensure no more than 20 threads are running simultaneously.
- Focus on optimizing the compression process using threads.

Building and Running
---------------------
1. Navigate to the `src` directory.
2. Use the provided Makefile to build the project:
    ```
    $ make
    ```
3. Run the executable:
    ```
    $ ./vzip <input_folder> <output_zip_file>
    ```

