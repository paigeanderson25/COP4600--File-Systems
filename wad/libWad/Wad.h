#ifndef WAD_H
#define WAD_H

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>
#include <map>

struct Node {
    std::string filename;
    size_t offset;
    size_t length;
    bool isDirectory;
    std::vector<Node*> children;
    //std::map<std::string, Node*> pathMap;
    

    Node(std::string filename, size_t offset, size_t length, bool isDirectory);
    ~Node() {
        for (Node* child: children) {
            delete child;
        }
    }
};

struct Descriptor {
    std::string name;
    size_t offset;
    size_t length;

    Descriptor(const std::string &name, size_t offset, size_t length);
    Descriptor();
};


class Wad {
    public:
    void printTree(const Node* node, const std::string& prefix = "");
    void printPathMap(const std::map<std::string, Node*>& pathMap);
    void shiftDescriptorsForSpace(size_t spaceNeeded);
    static Wad* loadWad(const std::string &path);
    ~Wad();
    std::string getMagic();
    bool isContent(const std::string &path);
    bool isDirectory(const std::string &path);
    int getSize(const std::string &path);
    int getContents(const std::string &path, char *buffer, int length, int offset = 0);
    int getDirectory(const std::string &path, std::vector<std::string> *directory);
    void createDirectory(const std::string &path);
    void createFile(const std::string &path);
    int writeToFile(const std::string &path, const char *buffer, int length, int offset = 0);

    Node* getRoot() { return root; }               
    const Node* getRoot() const { return root; }

    



    private:
    Wad(const std::string &path);
    std::fstream wad;
    std::string magic;
    std::vector<Descriptor> descriptors; 
    int numDescriptors = 0;
    int descriptorOffset = 0;
    Node* root;
    std::map<std::string, Node*> pathMap;
    
};

#endif // WAD_H