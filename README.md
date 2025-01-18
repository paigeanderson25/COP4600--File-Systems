# COP4600--File-Systems
This project involves the implementation of a userspace filesystem daemon.
The filesystem in memory consists of a tree node structure with directories/files. Each node stores name, size, offset, type and a vector of children. I also utilized a vector of Descriptor objects to easily access the descriptor list variables. Furthermore, I utilized a map of paths and nodes to access nodes through their paths
