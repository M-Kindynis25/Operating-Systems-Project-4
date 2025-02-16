/* MyzArchive.cpp */
#include "MyzArchive.hpp"
#include <iostream>
#include <ctime>
#include "MyzProcessing.hpp"
#include "TreeNode.hpp"

// Κατασκευαστής
MyzArchive::MyzArchive(char* in_archiveName, Vector<char*>& in_files, bool in_compress) {
    if (in_archiveName) {
        size_t len = std::strlen(in_archiveName) + 1; 
        archiveName = new char[len];
        std::memcpy(archiveName, in_archiveName, len);
    } else {
        // Αν το in_archiveName είναι nullptr, απλώς το ρυθμίζουμε σε nullptr
        archiveName = nullptr;
    }
    files = in_files;
    compress = in_compress;
}

// Καταστροφέας
MyzArchive::~MyzArchive() {
    // Αποδεσμεύουμε τη μνήμη που δεσμεύσαμε για το archiveName
    if (archiveName) {
        delete[] archiveName;
        archiveName = nullptr;
    }
}

// ----------------------------------------------------------------------------------------------------------
// Δημιουργία νέου .myz αρχείου (-c)
// ----------------------------------------------------------------------------------------------------------
void MyzArchive::createArchive() {
    // 1. Δημιουργία του αρχείου .myz, πριν από το writeHeaderToArchive;
    int fd = open(archiveName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        std::cerr << "Σφάλμα: Αποτυχία δημιουργίας του αρχείου " << archiveName << ".\n";
        return;
    }

    // 2. Γράφει το header στο offset=0 όπου ακόμη είναι κενό
    writeHeaderToArchive();     

    // 3. Θέτουμε το offset στο πρώτο byte των data και ενημερώνουμε το dataOffset
    off_t dataAreaStart = 512;
    lseek(fd, dataAreaStart, SEEK_SET);
    header.dataOffset = dataAreaStart;

    // 4. Διατρέχουμε τα paths που δόθηκαν
    for (size_t i = 0; i < files.get_size(); ++i) {
        processOne(files[i], fd, compress, dataAreaStart, nodeList);
    }

    // Αφού γράψαμε τα δεδομένα, φτάνουμε στο τέλος του Data Area
    off_t endOfData = lseek(fd, 0, SEEK_CUR);
    // Ενημερώνουμε στο header το metaOffset και το metaCount
    header.metaOffset = endOfData;        
    header.metaCount = nodeList.get_size();

    // 5. Γράφει τα nodeList στο offset=header.metaOffset
	writeNodeListToArchive(); 

    // 6. Ξαναγυρνάμε το header στο offset=0
	writeHeaderToArchive(); 

    // 7. Κλείνουμε το αρχείο
    close(fd);

    std::cout << "[DEBUG] createArchive: Ολοκληρώθηκε επιτυχώς.\n";
}


// ----------------------------------------------------------------------------------------------------------
// Προσθήκη σε υπάρχον .myz αρχείο (-a)
// ----------------------------------------------------------------------------------------------------------
void MyzArchive::addToArchive() {
    // 1. Χρήση της readArchive για να διαβάσουμε το Header και τους παλιούς κόμβους
    if (!readArchive()) {
        std::cerr << "Σφάλμα: Αποτυχία ανάγνωσης από το αρχείο " << archiveName << ".\n";
        return;
    }

    // 2. Άνοιγμα σε read-write
    int fd = open(archiveName, O_RDWR); 
    if (fd < 0) {
        std::cerr << "Αδυναμία ανοίγματος του " << archiveName << " για προσθήκη.\n";
        return;
    }

    // 3. Το παλιό metadata ξεκινάει στο header.metaOffset.
    //    Εκεί τελείωνε η παλιά Data Area.
    off_t dataAreaStart = header.metaOffset;

    // 4. Μετακινούμαστε στο τέλος της Data Area
    if (lseek(fd, dataAreaStart, SEEK_SET) == (off_t)-1) {
        std::cerr << "lseek dataAreaStart.\n";
        close(fd);
        return;
    }

    // 5. Για κάθε νέο αρχείο/φάκελο, γράφουμε τα data (processOne),
    //    προσθέτοντας κόμβους στο nodeList.
    off_t dataOffset = dataAreaStart; // θα μετακινείται από processOne
    for (size_t i = 0; i < files.get_size(); ++i) {
        processOne(files[i], fd, compress, dataOffset, nodeList);
    }

    // 6. Σημειώνουμε πού τελειώνει τώρα η Data Area
    off_t newEndOfData = lseek(fd, 0, SEEK_CUR);
    if (newEndOfData == (off_t)-1) {
        perror("lseek newEndOfData");
        close(fd);
        return;
    }

    // 7. Τα νέα metadata πρέπει να γραφούν μετά το νέοEndOfData
    header.metaOffset = newEndOfData;
    header.metaCount = nodeList.get_size();

    close(fd);

    // 8. Γράφουμε τους κόμβους (παλιούς+νέους) στο header.metaOffset:
    writeNodeListToArchive();

    // 9. Γράφουμε τον updated Header στο offset=0:
    writeHeaderToArchive();

    std::cout << "[DEBUG] addToArchive: Ολοκληρώθηκε επιτυχώς.\n";
}


// ----------------------------------------------------------------------------------------------------------
// Εξαγωγή αρχείων (-x)
// ----------------------------------------------------------------------------------------------------------
void MyzArchive::extractArchive() {
    // 1. Χρήση της readArchive για να διαβάσουμε το Header και τους παλιούς κόμβους
    if (!readArchive()) {
        std::cerr << "Σφάλμα: Αποτυχία ανάγνωσης του αρχείου .myz.\n";
        return;
    }

    // 2. Άνοιγμα του .myz αρχείου για ανάγνωση
    int fd = open(archiveName, O_RDONLY);
    if (fd < 0) {
        perror("open");
        std::cerr << "Αποτυχία ανοίγματος του αρχείου .myz\n";
        return;
    }


    // 3. Για κάθε κόμβο nodeList, ελέγχουμε αν θα τον εξαγάγουμε
    for (size_t i = 0; i < nodeList.get_size(); ++i) {
        const MyzNode& node = nodeList[i];

        // Αν είναι φάκελος
        if (S_ISDIR(node.st_mode)) {
            // Αν δεν ζητήθηκε συγκεκριμένη λίστα (ή αν περιέχει αυτόν τον φάκελο), τον φτιάχνουμε
            if (shouldExtract(files, node.path)) { 
                extractDirectory(node);
            }
        }
        else if (S_ISREG(node.st_mode)) {
            // Αν είναι αρχείο
            if (shouldExtract(files, node.path)) {
                extractFile(fd, node);
            }
        }
        // Μέσα στη συνάρτηση extractArchive, μετά τα if για S_ISDIR και S_ISREG:
        else if (S_ISLNK(node.st_mode)) {
            // Αν το node αντιστοιχεί σε symbolic link και πρέπει να εξαγάγεται:
            if (shouldExtract(files, node.path)) {
                extractSymbolicLink(node);
            }
        }
        else {
            // Εδώ θα μπορούσες να χειριστείς symlinks ή άλλους τύπους
            // ή να τους αγνοήσεις.
        }
    }

    // 4. Κλείσιμο του αρχείου
    close(fd);
    std::cout << "[DEBUG] extractArchive: Ολοκληρώθηκε επιτυχώς.\n";
}

// ----------------------------------------------------------------------------------------------------------
// Διαγραφή οντοτήτων από το .myz (-d)
// ----------------------------------------------------------------------------------------------------------
void MyzArchive::deleteFromArchive() {
    // 1. Διαβάζουμε header + παλιό nodeList
    if (!readArchive()) {
        std::cerr << "[ERROR] Αποτυχία ανάγνωσης του αρχείου .myz.\n";
        return;
    }

    // 2. Φτιάχνουμε updatedList που θα περιέχει ΟΛΟΥΣ τους κόμβους ΕΚΤΟΣ από αυτούς που διαγράφονται
    Vector<MyzNode> updatedList; 
    for (size_t j = 0; j < nodeList.get_size(); ++j) {
        const MyzNode& nd = nodeList[j];
        
        // Ελέγχουμε αν το nd.path ταιριάζει σε οποιοδήποτε από τα files που θέλουμε να σβήσουμε
        bool toDelete = false;
        for (size_t i = 0; i < files.get_size(); ++i) {
            if (strcmp(nd.path, files[i]) == 0) {
                toDelete = true;
                break;
            }
        }
        
        if (!toDelete) {
            // Δεν διαγράφεται -> το κρατάμε
            updatedList.push_back(nd);
        } else {
            std::cout << "[DEBUG] Διαγραφή εγγραφής για path: " << nd.path << "\n";
        }
    }

    // 3. Αντικαθιστούμε το nodeList της κλάσης με το updatedList
    nodeList = updatedList;

    // 4. Ενημερώνουμε το header.metaCount
    header.metaCount = nodeList.get_size();

    // 5. Καλούμε τη συνάρτηση που ξαναγράφει τη λίστα κόμβων (metadata) στο αρχείο, 
    writeNodeListToArchive();

    // 6. Τέλος, ξαναγράφουμε τον updated Header στο offset=0
    writeHeaderToArchive();

    std::cout << "[DEBUG] deleteFromArchive: Ολοκληρώθηκε επιτυχώς.\n";
}


// ----------------------------------------------------------------------------------------------------------
// Εκτύπωση μεταδεδομένων (-m)
// ----------------------------------------------------------------------------------------------------------
void MyzArchive::printMetadata() {
    // 1. Χρήση της readArchive για να διαβάσουμε το Header και τους παλιούς κόμβους
    if (!readArchive()) {
        std::cerr << "Σφάλμα: Αποτυχία ανάγνωσης του αρχείου .myz.\n";
        return;
    }

    // 2. Εκτύπωση όλων των MyzNodes
    for (size_t i = 0; i < nodeList.get_size(); ++i) {
        printNode(nodeList[i]);
    }

    std::cout << " printMetadata: Ολοκληρώθηκε επιτυχώς.\n";
}

// ----------------------------------------------------------------------------------------------------------
// Εκτύπωση ιεραρχίας (-p)
// ----------------------------------------------------------------------------------------------------------
void MyzArchive::printHierarchy() {
    // 1. Χρήση της readArchive για να διαβάσουμε το Header και τους παλιούς κόμβους
    if (!readArchive()) {
        std::cerr << "Σφάλμα: Αποτυχία ανάγνωσης του αρχείου .myz.\n";
        return;
    }

    // 2. Δημιουργία ρίζας του δέντρου
    TreeNode* root = new TreeNode("/", true, nullptr);

    // 3. Εισαγωγή όλων των paths στο δέντρο
    for (size_t i = 0; i < nodeList.get_size(); ++i) {
        const MyzNode& node = nodeList[i];
        // Αν το node είναι κατάλογος, το isDirectory είναι true
        // Διαφορετικά, είναι αρχείο
        bool isDir = S_ISDIR(node.st_mode);
        root->insertPath(node.path, isDir);
    }

    // 4. Εκτύπωση του δέντρου ξεκινώντας από τη ρίζα
    root->printTree(0);

    // 5. Καθαρισμός του δέντρου
    delete root;

    std::cout << " printHierarchy: Ολοκληρώθηκε επιτυχώς.\n";
}

// ----------------------------------------------------------------------------------------------------------
// Ερώτηση ύπαρξης στοιχείων (-q)
// ----------------------------------------------------------------------------------------------------------
void MyzArchive::queryArchive() {
    // 1. Χρήση της readArchive για να διαβάσουμε το Header και τους παλιούς κόμβους
    if (!readArchive()) {
        std::cerr << "Σφάλμα: Αποτυχία ανάγνωσης του αρχείου .myz.\n";
        return;
    }
    // 2. Για κάθε στοιχείο στη λίστα, ελέγχουμε αν υπάρχει στα metadata
    for (size_t i = 0; i < files.get_size(); ++i) {
        const char* queryPath = files[i];
        bool found = false;

        for (size_t j = 0; j < nodeList.get_size(); ++j) {
            if (strcmp(queryPath, nodeList[j].path) == 0) {
                found = true;
                break;
            }
        }

        if (found) {
            std::cout << "Found: " << queryPath << "\n";
        }
        else {
            std::cout << "Not found: " << queryPath << "\n";
        }
    }

    std::cout << " queryArchive: Ολοκληρώθηκε επιτυχώς.\n";
}

// ---------------------------------------------------------------------------
// readArchive: Διαβάζει το Header και όλα τα MyzNodes από το αρχείο .myz
// ---------------------------------------------------------------------------
bool MyzArchive::readArchive() {
    // 1. Άνοιγμα του .myz αρχείου για ανάγνωση
    int fd = open(archiveName, O_RDONLY);
    if (fd < 0) {
        perror("open");
        std::cerr << "Αποτυχία ανοίγματος του αρχείου .myz\n";
        return false;
    }

    // 2. Ανάγνωση του Header
    ssize_t bytesRead = read(fd, &header, sizeof(header));
    if (bytesRead != sizeof(header)) {
        std::cerr << "Σφάλμα: Αποτυχία ανάγνωσης του Header.\n";
        close(fd);
        return false;
    }

    // 3. Έλεγχος του magic number
    if (std::memcmp(header.magic, "MYZ!", 4) != 0) {
        std::cerr << "Σφάλμα: Το αρχείο δεν είναι έγκυρο .myz (invalid magic).\n";
        close(fd);
        return false;
    }

    // 4. Μετακίνηση στο metaOffset
    if (lseek(fd, header.metaOffset, SEEK_SET) == (off_t)-1) {
        perror("lseek to metaOffset");
        std::cerr << "Σφάλμα: Αποτυχία μετακίνησης στο Metadata Offset.\n";
        close(fd);
        return false;
    }

    // 5. Ανάγνωση και αποθήκευση όλων των MyzNodes
    for (uint64_t i = 0; i < header.metaCount; ++i) {
        MyzNode node;
        ssize_t r = read(fd, &node, sizeof(node));
        if (r != sizeof(node)) {
            std::cerr << "Σφάλμα: Αποτυχία ανάγνωσης του MyzNode #" << i+1 << ".\n";
            close(fd);
            return false;
        }
        nodeList.push_back(node);
    }

    // 6. Κλείσιμο του αρχείου
    close(fd);

    return true;
}

// ---------------------------------------------------------------------------
// modeToString: Μετατρέπει το mode σε συμβολική μορφή rwxr-xr-x
// ---------------------------------------------------------------------------
void MyzArchive::modeToString(mode_t mode, char* str) {
    str[0] = (mode & S_IRUSR) ? 'r' : '-';
    str[1] = (mode & S_IWUSR) ? 'w' : '-';
    str[2] = (mode & S_IXUSR) ? 'x' : '-';
    str[3] = (mode & S_IRGRP) ? 'r' : '-';
    str[4] = (mode & S_IWGRP) ? 'w' : '-';
    str[5] = (mode & S_IXGRP) ? 'x' : '-';
    str[6] = (mode & S_IROTH) ? 'r' : '-';
    str[7] = (mode & S_IWOTH) ? 'w' : '-';
    str[8] = (mode & S_IXOTH) ? 'x' : '-';
    str[9] = '\0';
}

// ---------------------------------------------------------------------------
// printHeader: Εκτυπώνει τα περιεχόμενα του MyzHeader
// ---------------------------------------------------------------------------
void MyzArchive::printHeader(const MyzHeader& header) {
    std::cout << "----- MyzHeader -----\n";
    std::cout << "Magic: ";
    for(int i = 0; i < 4; ++i) {
        std::cout << header.magic[i];
    }
    std::cout << "\n";
    std::cout << "Version: " << header.version << "\n";
    std::cout << "Data Offset: " << header.dataOffset << "\n";
    std::cout << "Metadata Offset: " << header.metaOffset << "\n";
    std::cout << "Metadata Count: " << header.metaCount << "\n";
    std::cout << "---------------------\n";
}

// ---------------------------------------------------------------------------
// printNode: Εκτυπώνει τα περιεχόμενα ενός MyzNode
// ---------------------------------------------------------------------------
void MyzArchive::printNode(const MyzNode& node) {
    std::cout << "----- MyzNode -----\n";
    std::cout << "Path: " << node.path << "\n";
    
    // Εκτύπωση Permissions σε μορφή rwxr-xr-x
    char permStr[10];
    modeToString(node.st_mode, permStr);
    std::cout << "Permissions: " << permStr << "\n";
    
    // Τύπος αρχείου
    if (S_ISDIR(node.st_mode)) {
        std::cout << "Type: Directory\n";
    }
    else if (S_ISREG(node.st_mode)) {
        std::cout << "Type: Regular File\n";
    }
    else if (S_ISLNK(node.st_mode)) {
        std::cout << "Type: Symbolic Link\n";
        std::cout << "Symlink Target: " << node.symlinkTarget << "\n";
    }
    else {
        std::cout << "Type: Other\n";
    }
    
    std::cout << "Data Offset: " << node.dataOffset << "\n";
    std::cout << "Data Size: " << node.dataSize << " bytes\n";
    std::cout << "Original Size: " << node.originalSize << " bytes\n";
    std::cout << "Is Compressed: " << (node.isCompressed ? "Yes" : "No") << "\n";

    std::cout << "Inode Number: " << node.inode << "\n";
    std::cout << "Owner UID: " << node.uid << "\n";
    std::cout << "Group GID: " << node.gid << "\n";
    
    // Εκτύπωση Χρονικών Σφραγίδων
    std::cout << "Timestamps:\n";
    std::cout << "  Created: " << std::ctime(&node.creTime);
    std::cout << "  Modified: " << std::ctime(&node.modTime);
    std::cout << "  Accessed: " << std::ctime(&node.accTime);
    std::cout << "--------------------\n\n";
}

// ---------------------------------------------------------------------------
// writeHeaderToArchive: Γράφει το header στο offset=0
// ---------------------------------------------------------------------------
void MyzArchive::writeHeaderToArchive() {
    // 1. Άνοιγμα του αρχείου σε read-write
    int fd = open(archiveName, O_RDWR);
    if (fd < 0) {
        perror("open");
        std::cerr << "[ERROR] Αδυναμία ανοίγματος του αρχείου .myz για εγγραφή Header.\n";
        return;
    }

    // 2. Μετακίνηση στο offset 0 (αρχή του αρχείου)
    off_t pos = lseek(fd, 0, SEEK_SET);
    if (pos == (off_t)-1) {
        perror("lseek");
        std::cerr << "[ERROR] lseek(0) απέτυχε.\n";
        close(fd);
        return;
    }

    // 3. Γράψιμο του Header
    ssize_t w = write(fd, &(header), sizeof(header));
    if (w != (ssize_t)sizeof(header)) {
        perror("write header");
        std::cerr << "[ERROR] Αδυναμία εγγραφής του Header στο αρχείο .myz\n";
        close(fd);
        return;
    }

    // 4. Γεμίζει τα υπόλοιπα byte έως 512 με μηδενικά
    if (sizeof(header) < 512) {
        char pad[512 - sizeof(header)];
        std::memset(pad, 0, sizeof(pad));
        write(fd, pad, sizeof(pad));
    }

    // 5. Κλείσιμο
    close(fd);
}

// ---------------------------------------------------------------------------
// writeNodeListToArchive: Γράφει τα nodeList στο offset=header.metaOffset
// ---------------------------------------------------------------------------
void MyzArchive::writeNodeListToArchive() {
    // 1. Άνοιγμα αρχείου σε read-write
    int fd = open(archiveName, O_RDWR);
    if (fd < 0) {
        perror("open");
        std::cerr << "[ERROR] Αδυναμία ανοίγματος του αρχείου .myz για εγγραφή nodeList.\n";
        return;
    }

    // 2. Μετακίνηση στο offset όπου θα γράψουμε τα metadata
    off_t pos = lseek(fd, header.metaOffset, SEEK_SET);
    if (pos == (off_t)-1) {
        perror("lseek metaOffset");
        std::cerr << "[ERROR] lseek(header.metaOffset) απέτυχε.\n";
        close(fd);
        return;
    }

    // 3. Γράψιμο όλων των κόμβων στο nodeList
    for (size_t i = 0; i < nodeList.get_size(); ++i) {
        MyzNode& nd = nodeList[i];
        ssize_t w = write(fd, &nd, sizeof(nd));
        if (w != (ssize_t)sizeof(nd)) {
            perror("write node");
            std::cerr << "[ERROR] Αδυναμία εγγραφής κόμβου #" << i << " στο .myz\n";
            close(fd);
            return;
        }
    }

    // 4. Κλείσιμο
    close(fd);
}
