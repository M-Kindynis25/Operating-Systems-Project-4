/* TreeNode.hpp */
#include "vector.hpp"
#include <iostream>
#include <cstring>

class TreeNode {
private:
    char name[256];
    bool isDirectory;
    TreeNode* parent;
    Vector<TreeNode*> children;

public:
    // Constructor
    TreeNode(const char* nodeName, bool dir, TreeNode* parentNode = nullptr)
        : isDirectory(dir), parent(parentNode) {
        strncpy(name, nodeName, sizeof(name));
        name[sizeof(name) - 1] = '\0';
    }

    // Destructor
    ~TreeNode() {
        for (size_t i = 0; i < children.get_size(); ++i) {
            delete children[i];
        }
    }

    // Προσπέλαση δεδομένων με getters
    const char* getName() const { return name; }
    bool getIsDirectory() const { return isDirectory; }

    // Προσθήκη παιδιού
    void addChild(TreeNode* child) {
        children.push_back(child);
    }

    // Εισαγωγή path
    void insertPath(const char* path, bool isDir) {
        char tempPath[256];
        strncpy(tempPath, path, sizeof(tempPath));
        tempPath[sizeof(tempPath) - 1] = '\0';

        char* token = strtok(tempPath, "/");
        TreeNode* current = this;

        while (token != nullptr) {
            TreeNode* child = current->findChild(token);
            if (child == nullptr) {
                bool dir = (strtok(NULL, "/") != nullptr) ? true : isDir;
                if (dir && strtok(NULL, "/") != nullptr) {
                    dir = true;
                    strtok(NULL, "/"); // Επιστροφή στο προηγούμενο state
                }
                child = new TreeNode(token, dir, current);
                current->addChild(child);
            }
            current = child;
            token = strtok(nullptr, "/");
        }
    }

    // Αναδρομική εκτύπωση
    void printTree(int depth = 0) const {
        for (int i = 0; i < depth; ++i) {
            std::cout << "  ";
        }
        std::cout << "|-- " << name;
        if (isDirectory) {
            std::cout << "/\n";
            for (size_t i = 0; i < children.get_size(); ++i) {
                children[i]->printTree(depth + 1);
            }
        } else {
            std::cout << "\n";
        }
    }

private:
    // Εύρεση παιδιού με συγκεκριμένο όνομα
    TreeNode* findChild(const char* childName) const {
        for (size_t i = 0; i < children.get_size(); ++i) {
            if (strcmp(children[i]->name, childName) == 0) {
                return children[i];
            }
        }
        return nullptr;
    }
};
