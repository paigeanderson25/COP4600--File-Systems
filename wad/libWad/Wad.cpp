#include "Wad.h"
#include <iostream>
#include <cstring>
#include <stack>
#include <regex>


Node::Node(std::string filename, size_t offset, size_t length, bool isDirectory) : filename(filename), offset(offset), length(length), isDirectory(isDirectory) {}

Descriptor::Descriptor(const std::string &name, size_t offset, size_t length) {
    this->name = name;
    this->offset = offset;
    this->length = length;

}
Descriptor::Descriptor() {
    this->name = "";
    this->offset = 0;
    this->length = 0;
}

// open wad file, load file, set member variables and build tree by parsing descriptors
Wad::Wad(const std::string &path) {
    // open file
    wad.open(path, std::ios::in | std::ios::out | std::ios::binary);
    pathMap.clear();
 
    // read in header content & set variables
    char magic[4];
    wad.read(magic, 4);
    this->magic = std::string(magic, 4);
    wad.read(reinterpret_cast<char*>(&numDescriptors), 4);
    wad.read(reinterpret_cast<char*>(&descriptorOffset), 4);

    // read descriptor list
    wad.seekg(descriptorOffset, std::ios::beg);
    descriptors.resize(numDescriptors);
    for (int i = 0; i < numDescriptors; ++i) {
        Descriptor desc;
        wad.read(reinterpret_cast<char*>(&desc.offset), 4);
        wad.read(reinterpret_cast<char*>(&desc.length), 4);
        char name[8];
        wad.read(name, 8);
        desc.name = std::string(name, strnlen(name, 8));
        descriptors[i] = desc;
        //td::cout << "Descriptor " << i << ": Name: " << desc.name << " Offset: " << desc.offset << " Length: " << desc.length << std::endl;
    }

    // create stack and set root node
    std::stack<Node*> dirStack;
    root = new Node{"root", 0, 0, true};
    dirStack.push(root);

    // use regex for directory matching
    std::regex mapMarkerRegex(R"(E[0-9]M[0-9])");
    std::regex namespaceStartRegex(R"((.+)_START)");
    std::regex namespaceEndRegex(R"((.+)_END)");

    pathMap["/"] = root;

    // iterate through descriptors and handle directory types and plain files (4 cases)
    for (size_t i = 0; i < descriptors.size(); ++i) {
        auto& desc = descriptors[i];
        Node* currDir = dirStack.top();

        // Reconstruct the current path from the directory stack
        std::string currPath = "/";
        std::stack<Node*> tempStack = dirStack;
        std::vector<std::string> pathComponents;

        while (!tempStack.empty()) {
            pathComponents.push_back(tempStack.top()->filename);
            tempStack.pop();
        }

        for (auto it = pathComponents.rbegin(); it != pathComponents.rend(); ++it) {
            if (*it != "root") {
                currPath += *it + "/";
            }
        }

        // Check map markers
        if (std::regex_match(desc.name, mapMarkerRegex)) {
            Node* mapDir = new Node{desc.name, 0, 0, true};
            pathMap[currPath + mapDir->filename + "/"] = mapDir;
            currDir->children.push_back(mapDir);
            dirStack.push(mapDir);

            // Read in next 10 descriptors and add to tree
            for (int j = 0; j < 10; ++j) {
                if (++i >= descriptors.size()) {
                    //std::cout << "max descriptors reached" << std::endl;
                    break;
                }
                const auto& mapDesc = descriptors[i];
                Node* mapNode = new Node{mapDesc.name, mapDesc.offset, mapDesc.length, false};
                mapDir->children.push_back(mapNode);
                pathMap[currPath + mapDir->filename + "/" + mapNode->filename] = mapNode;
            }
            dirStack.pop();
        }
        // Check namespace start markers
        else if (std::regex_search(desc.name, namespaceStartRegex)) {
            size_t pos = desc.name.find("_START");
            std::string truncatedName = (pos != std::string::npos) ? desc.name.substr(0, pos) : desc.name;
            Node* nsDir = new Node{truncatedName, 0, 0, true};
            currDir->children.push_back(nsDir);
            dirStack.push(nsDir);
            pathMap[currPath + nsDir->filename + "/"] = nsDir;

            //std::cout << "Directory created: " << currPath + nsDir->filename << std::endl;
        }
        // Check namespace end markers
        else if (std::regex_search(desc.name, namespaceEndRegex)) {
            dirStack.pop();
        }
        // Handle regular files
        else {
            Node* fileNode = new Node{desc.name, desc.offset, desc.length, false};
            currDir->children.push_back(fileNode);
            pathMap[currPath + fileNode->filename] = fileNode;
        }
    }
    //std::cout << "Tree end constructor:" << std::endl;
    //printTree(root);
}


Wad* Wad::loadWad(const std::string &path) {
    return new Wad(path);
}

Wad::~Wad() {
    if (root != nullptr) {
        delete root;
    } 

    if (wad.is_open()) {
        wad.flush();
    }

    pathMap.clear();
    numDescriptors = 0;
    descriptorOffset = 0;      
}

std::string Wad::getMagic() {
    return magic; 
}

bool Wad::isContent(const std::string &path) {
// Takes in path to a file in your WAD filesystem.
// Will return true if it is a valid path to an existing content file.
// Will return false if it is a valid path to a directory, or if the path is invalid (nonexistent)

    auto it = pathMap.find(path);
    //std::cout << "Searching for path: " << path << std::endl;
    if (it != pathMap.end()) {
        Node* node = it->second;
        //std::cout << "Found path: " << path << ", isDirectory: " << (node->isDirectory ? "true" : "false") << std::endl;
        return !node->isDirectory;
    }
    //std::cout << "Path content not found: " << path << std::endl;
    return false;
}

bool Wad::isDirectory(const std::string &path) {
// Similar to above, but will return true for valid directories, and false for content files/nonexistent paths
    std::string normPath = path;


    if (normPath.back() != '/' && !normPath.empty()) {
        //std::cout << "entered" << std::endl;
        normPath += "/";
    }
    auto it = pathMap.find(normPath);

    if (it != pathMap.end()) {
        Node* node = it->second;
        //std::cout << "Found path: " << path << ", isDirectory: " << (node->isDirectory ? "true" : "false") << std::endl;
        return node->isDirectory;
    }
    normPath = path;
    auto itWithoutSlash = pathMap.find(normPath);
    if (itWithoutSlash != pathMap.end()) {
        return itWithoutSlash->second->isDirectory;
    }

    return false;
}

int Wad::getSize(const std::string &path) {
// Returns the size of the file at path. If path is points to a directory or is invalid, returns -1.
    auto it = pathMap.find(path);
    //std::cout << "Searching for path: " << path << std::endl;
    //printPathMap(pathMap);
    if (it != pathMap.end()) {
        Node* node = it->second;
        if (!node->isDirectory) {
            return static_cast<int>(node->length);
        }
    }
    return -1;
}

int Wad::getContents(const std::string &path, char *buffer, int length, int offset) {
// Given a valid path to an existing content file, it will read length amount of bytes from the file’s lump data,
// starting at offset. Returns amount of bytes successfully copied. Returns -1 if path is directory/invalid.

    auto it = pathMap.find(path);
    if (it != pathMap.end()) {
        Node* node = it->second;
        if (node->isDirectory) {
            return -1;
        }
        if (offset >= static_cast<int>(node->length)) {
            return 0;
        }
        

        int bytesToCopy = std::min(length, static_cast<int>(node->length) - offset);
        wad.seekg(node->offset + offset, std::ios::beg);
        wad.read(buffer, bytesToCopy);
        return bytesToCopy;
        
    }
    return -1;
}

int Wad::getDirectory(const std::string &path, std::vector<std::string> *directory) {
// Takes in path to a directory, and pushes back the names of all the directory’s children into the passed in vector.
// Returns the amount of children copied into vector

    std::string normPath = path;
    if (!normPath.empty() && normPath.back() != '/') {
        normPath += "/";
    }

    auto it = pathMap.find(normPath);
    if (it == pathMap.end() || !it->second->isDirectory) {
        return -1;
    }

    Node* dirNode = it->second;
    for (const Node* child : dirNode->children) {
        directory->push_back(child->filename);
    }

    return directory->size();

}



void Wad::createDirectory(const std::string &path) {
    //std::cout << "Attempting to create directory: " << path << std::endl;

    // Trim trailing slash if present, except for root "/"
    std::string trimPath = path;
    if (path.length() > 1 && path.back() == '/') {
        trimPath.pop_back();
    }
    //std::cout << "Trimmed Path: " << trimPath << std::endl;
    
    // Check if path is valid
    if (trimPath.empty() || trimPath[0] != '/') {
        std::cout << "Invalid path: Must be non-empty and start with '/'" << std::endl;
        return;
    }

    // Handle root path specifically
    if (trimPath == "/") {
        std::cout << "Cannot create root directory: it already exists" << std::endl;
        return;
    }

    // Find the position of the last '/'
    size_t pos = trimPath.find_last_of('/');
    if (pos == std::string::npos || pos == trimPath.length() - 1) {
        std::cout << "Invalid path: Must have a valid parent directory and directory name" << std::endl;
        return;
    }

    std::string parentPath = trimPath.substr(0, pos);
    std::string dirName = trimPath.substr(pos + 1);

    // Handle root
    if (parentPath.empty()) {
        parentPath = "/";
        dirName = dirName + "";
    }
    else {
        parentPath = parentPath + "/";
        dirName = dirName + "";
    }

    //std::cout << "Parent path: " << parentPath << ", Directory name: " << dirName << std::endl;

    // Ensure parent directory exists
    auto it = pathMap.find(parentPath);
    if (it == pathMap.end()) {
        //std::cout << "Parent directory does not exist: " << parentPath << std::endl;
        return;
    }
    if (!it->second->isDirectory) {
        //std::cout << "Parent path is not a directory: " << parentPath << std::endl;
        return;
    }

    // Ensure directory name is 2 or less characters
    if (dirName.length() > 2) {
        //std::cout << "Invalid directory name: " << dirName << " (must be at most 2 characters)" << std::endl;
        return;
    }

    std::regex mapMarkerRegex(R"(/E[0-9]M[0-9]/)");
    if (std::regex_match(parentPath, mapMarkerRegex)) {
        return;
    }

    Node* parentDir = it->second;

    // Special case for the root directory
    if (parentPath == "/") {
        //std::cout << "Root directory: No '_END' descriptor needed." << std::endl;
        
        // Insert new descriptors after the root, no need to look for '_END'
        Descriptor startDesc(dirName + "_START", 0, 0);
        Descriptor endDesc(dirName + "_END", 0, 0);

        shiftDescriptorsForSpace(32);

        // Insert descriptors at the end of the list
        descriptors.push_back(startDesc);
        descriptors.push_back(endDesc);

        // Update the data structures
        Node* newDir = new Node(dirName, 0, 0, true);
        parentDir->children.push_back(newDir);
        pathMap[trimPath + "/"] = newDir;

        numDescriptors += 2;
        // Write the updated descriptors to the file at the correct location
        wad.seekp(descriptorOffset, std::ios::beg); 
        for (const auto& desc : descriptors) {
            wad.write(reinterpret_cast<const char*>(&desc.offset), 4);
            wad.write(reinterpret_cast<const char*>(&desc.length), 4);
            wad.write(desc.name.c_str(), 8);
        }
        if (!wad) {
            std::cout << "Failed to write descriptors to the WAD file" << std::endl;
        }            

        wad.seekp(4);
        wad.write(reinterpret_cast<const char*>(&numDescriptors), sizeof(numDescriptors));

        return;
    }



    // Create start and end descriptors for the new directory
    Descriptor startDesc(dirName + "_START", 0, 0);
    Descriptor endDesc(dirName + "_END", 0, 0);

    shiftDescriptorsForSpace(32);

    // Find the position to insert the new descriptors
    auto endIt = std::find_if(descriptors.begin(), descriptors.end(), [&](const Descriptor& desc) {
        return desc.name == parentDir->filename + "_END";
    });

    if (endIt == descriptors.end()) {
        std::cout << "Parent directory '_END' descriptor not found: " 
                  << parentDir->filename + "_END" << std::endl;
        return;
    }


    // Insert the new descriptors
    auto startIt = descriptors.insert(endIt, startDesc);
    descriptors.insert(startIt + 1, endDesc);

    //std::cout << "Descriptors inserted successfully" << std::endl;

    // Update the data structures
    Node* newDir = new Node(dirName, 0, 0, true);
    parentDir->children.push_back(newDir);
    pathMap[trimPath + "/"] = newDir;

    // Increment the number of descriptors in the header
    numDescriptors += 2;

    //std::cout << "Directory created successfully: " << trimPath << std::endl;

    wad.seekp(descriptorOffset, std::ios::beg);
    for (const auto& desc : descriptors) {
        wad.write(reinterpret_cast<const char*>(&desc.offset), 4);
        wad.write(reinterpret_cast<const char*>(&desc.length), 4);
        wad.write(desc.name.c_str(), 8); 
    }

    if (!wad) {
        std::cout << "Failed to write descriptors to the WAD file" << std::endl;
    }

    // Update the descriptor count in the header
    wad.seekp(4);
    wad.write(reinterpret_cast<const char*>(&numDescriptors), sizeof(numDescriptors));

    if (wad.is_open()) {
        wad.flush();
    }
    
}

void Wad::shiftDescriptorsForSpace(size_t spaceNeeded) {
    size_t numBytesToShift = spaceNeeded;
    size_t shiftAmount = numBytesToShift / sizeof(Descriptor);
    descriptors.resize(descriptors.size() + shiftAmount); 
    std::copy_backward(descriptors.begin(), descriptors.end() - shiftAmount, descriptors.end());
}



void Wad::createFile(const std::string &path) {
    // Find the position of the last '/' to separate the parent path and the new file name
    size_t pos = path.find_last_of('/');
    if (pos == std::string::npos || pos == path.length() - 1) {
        throw std::invalid_argument("Invalid path");
    }

    std::string parentPath = path.substr(0, pos);
    std::string fileName = path.substr(pos + 1);

    // Special case: If the parent path is empty, it means the parent is the root "/"
    if (parentPath.empty()) {
        parentPath = "/";
    }
    else {
        parentPath = parentPath + "/";
    }

    // Ensure the parent directory exists
    auto it = pathMap.find(parentPath);
    if (it == pathMap.end() || !it->second->isDirectory) {
        return;
        throw std::invalid_argument("Parent directory does not exist or is not a directory");
    }


    std::regex mapMarkerRegex(R"(E[0-9]M[0-9])");
    // Ensure the filename does not contain illegal sequences
    if (fileName.find("_START") != std::string::npos || fileName.find("_END") != std::string::npos ||
        std::regex_search(fileName, mapMarkerRegex)) {
            return;
        throw std::invalid_argument("Filename contains illegal sequences");
    }  
    if (std::regex_search(parentPath, mapMarkerRegex)) {
        return;
    }
    
    if (fileName.length() > 8) {
        return;
    }

    Node* parentDir = it->second;
    // Special case for the root directory
    if (parentPath == "/") {
        //std::cout << "Root directory: No '_END' descriptor needed." << std::endl;
        
        // Insert new descriptors after the root, no need to look for '_END'
        Descriptor startDesc(fileName, 0, 0);

        shiftDescriptorsForSpace(16);

        //std::cout << "Descriptors shifted" << std::endl;

        // Insert descriptors at the end of the list
        descriptors.push_back(startDesc);

        // Update the data structures
        Node* newFile = new Node(fileName, 0, 0, false);
        parentDir->children.push_back(newFile);
        pathMap["/" + fileName] = newFile;

        numDescriptors += 1;
        // Write the updated descriptors to the file at the correct location
        wad.seekp(descriptorOffset, std::ios::beg); // assuming descriptorOffset is the position where descriptors start
        for (const auto& desc : descriptors) {
            wad.write(reinterpret_cast<const char*>(&desc.offset), 4);
            wad.write(reinterpret_cast<const char*>(&desc.length), 4);
            wad.write(desc.name.c_str(), 8);
        }
        if (!wad) {
            std::cout << "Failed to write descriptors to the WAD file" << std::endl;
        }            

        wad.seekp(4);
        wad.write(reinterpret_cast<const char*>(&numDescriptors), sizeof(numDescriptors));


        //std::cout << "File created successfully: " << path << std::endl;
        return;
    }

    // Find the position to insert the new descriptor (before the parent directory's "_END" descriptor)
    auto endIt = std::find_if(descriptors.begin(), descriptors.end(), [&](const Descriptor& desc) {
        return desc.name == parentDir->filename + "_END";
    });

    if (endIt == descriptors.end()) {
        throw std::runtime_error("Parent directory's _END descriptor not found");
    }

    // Create a descriptor for the new file with an initial offset and length of 0
    Descriptor fileDesc(fileName, 0, 0);

    // Shift the rest of the descriptor list 
    shiftDescriptorsForSpace(16);

    // Insert the new descriptor before the "_END" descriptor
    descriptors.insert(endIt, fileDesc);

    // Update the data structures
    Node* newFile = new Node(fileName, 0, 0, false);
    parentDir->children.push_back(newFile);
    pathMap[path] = newFile;

    // Increment the number of descriptors in the header
    numDescriptors += 1;
    //std::cout << "File created successfully: " << path << std::endl;

    wad.seekp(descriptorOffset, std::ios::beg);
    for ( auto& desc : descriptors) {
        wad.write(reinterpret_cast<const char*>(&desc.offset), 4);
        wad.write(reinterpret_cast<const char*>(&desc.length), 4);
        wad.write(desc.name.c_str(), 8); // Ensure fixed-size writes
    }

    if (!wad) {
        std::cout << "Failed to write descriptors to the WAD file" << std::endl;
    }

    // Update the descriptor count in the header
    wad.seekp(4);
    wad.write(reinterpret_cast<const char*>(&numDescriptors), sizeof(numDescriptors));

    if (wad.is_open()) {
        wad.flush();
    }

}

int Wad::writeToFile(const std::string &path, const char *buffer, int length, int offset) {
    
    // Find the node at path
    auto it = pathMap.find(path);
    if (it == pathMap.end()) {
        return -1; 
    }

    Node* node = it->second;
    if (node->isDirectory) {
        return -1; 
    }

    if (node->length > 0) {
        return 0;
    }

    if (offset < 0 || offset > node->length) {
        return -1;
    }

    if (!wad.is_open()) {
        return -1;
    }
    //wad.seekg(0, std::ios::end);
    //std::streamsize size = wad.tellg();


    //std::cout << "Initial file size: " << size << std::endl;

    int lumpData = descriptorOffset;
    node->length = length;
    node->offset = descriptorOffset;
    //std::cout << "New node offset: " << node->offset << " and length: " << node->length << std::endl;


    for (auto& desc : descriptors) {
        if (desc.name == node->filename) {
            desc.length = node->length;
            desc.offset = node->offset;
            //std::cout << "descriptor node updated" << std::endl;
        }
    }

    wad.seekp(descriptorOffset + length, std::ios::beg);
    for (auto& desc : descriptors) {   
        wad.write(reinterpret_cast<const char*>(&desc.offset), 4);
        wad.write(reinterpret_cast<const char*>(&desc.length), 4);
        wad.write(desc.name.c_str(), 8);
    }
    wad.seekp(lumpData, std::ios::beg);
    wad.write(buffer, length);

    descriptorOffset += length;    

    // Update the header 
    wad.seekp(8, std::ios::beg);
    wad.write(reinterpret_cast<char*>(&descriptorOffset), sizeof(descriptorOffset));
    pathMap[path] = node;    

    if (wad.is_open()) {
        wad.flush();
    }

    return length;
    
}


void Wad::printTree(const Node* node, const std::string& prefix) {
    if (!node) return;

    // Print the node's name with indent
    std::cout << prefix << node->filename << "\n";


    // Recursively print each child 
    for (const auto& child : node->children) {
        printTree(child, prefix + "  ");
    }
}

void Wad::printPathMap(const std::map<std::string, Node*>& pathMap) {
    std::cout << "PathMap Contents:\n";
    for (const auto& [path, node] : pathMap) {
        // Print the path and the node name
        std::cout << path << " -> " 
                  << (node->isDirectory ? "[DIR] " : "[FILE] ") 
                  << node->filename << std::endl;
    }
}



