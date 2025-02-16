/* MyzProcessing.hpp */
#include "MyzArchive.hpp"
#include "vector.hpp"
#include <iostream>
#include <cstdlib>
#include <limits.h> 
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

#include "ReadWriteData.hpp"

// -------------------------------------------------------------------------------------------------------
// processOne:
//   1) Ελέγχει τι είδους path είναι (φάκελος, αρχείο, συμβολικός σύνδεσμος κλπ.).
//   2) Ανάλογα καλεί την αντίστοιχη process συνάρτηση (processDirectory, processFile, processSymbolicLink).
//   3) Αγνοεί άλλους τύπους (π.χ. συσκευές, sockets).
// -------------------------------------------------------------------------------------------------------
bool MyzArchive::processOne(const char* path, int fd, bool compress, off_t& dataOffset, Vector<MyzNode>& nodeList) {
    struct stat st;
    if (lstat(path, &st) < 0) {
        std::perror("lstat");
        std::cerr << "[WARN] Αδυναμία πρόσβασης σε: " << path << "\n";
        return false;
    }

    if (S_ISDIR(st.st_mode)) {
        // Είναι φάκελος
        processDirectory(path, fd, compress, dataOffset, nodeList);
    }
    else if (S_ISREG(st.st_mode)) {
        // Είναι κανονικό αρχείο
        processFile(path, fd, compress, dataOffset, nodeList);
    }
    else if (S_ISLNK(st.st_mode)) {
        // Είναι συμβολικός σύνδεσμος
        processSymbolicLink(path, nodeList);
    }
    else {
        // Άλλου τύπου (device, socket, FIFO κ.λπ.)
        std::cout << "[DEBUG] " << path << " είναι άλλου τύπου.\n";
    }
    return true;
}

// -------------------------------------------------------------------------------------------------------
// processDirectory: 
//   1) Προσθέτει ένα MyzNode για τον φάκελο στο nodeList (με dataSize = 0).
//   2) Έπειτα, διαβάζει τα περιεχόμενα του φακέλου και καλεί αναδρομικά την processOne.
// -------------------------------------------------------------------------------------------------------
void MyzArchive::processDirectory(const char* dirPath, int fd, bool compress, off_t& dataOffset, Vector<MyzNode>& nodeList) {
    struct stat sb;
    if (lstat(dirPath, &sb) == -1) {
        std::perror("lstat");
        return;
    }

    // 1) Φτιάχνουμε ένα MyzNode για τον φάκελο
    MyzNode dirNode;
    std::strcpy(dirNode.path, dirPath);
    dirNode.st_mode = sb.st_mode;
    dirNode.uid = sb.st_uid;
    dirNode.gid = sb.st_gid;
    dirNode.inode = sb.st_ino;
    dirNode.dataOffset = 0;
    dirNode.dataSize = 0;
    dirNode.originalSize = 0;
    dirNode.isCompressed = false;
    dirNode.modTime = sb.st_mtime;
    dirNode.accTime = sb.st_atime;
    dirNode.creTime = sb.st_ctime;

    nodeList.push_back(dirNode);

    // 2) Ανοίγουμε τον φάκελο
    DIR* dp = opendir(dirPath);
    if (!dp) {
        std::perror("opendir");
        std::cerr << "[WARN] Αδυναμία ανοίγματος φακέλου: " << dirPath << "\n";
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dp)) != nullptr) {
        // Αγνοούμε τις ειδικές εγγραφές "." και ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Φτιάχνουμε νέο path:  π.χ.  /home/user/dirA + /subdir
        char newPath[1024];
        std::snprintf(newPath, sizeof(newPath), "%s/%s", dirPath, entry->d_name);

        std::cout << "The path is: " << newPath << std::endl;

        // Αναδρομική κλήση
        processOne(newPath, fd, compress, dataOffset, nodeList);
    }

    closedir(dp);
}

// -------------------------------------------------------------------------------------------------------
// processFile: 
//   1) Διαβάζει τα metadata του αρχείου (μέγεθος, timestamps, άδειες).
//   2) Γράφει τα δεδομένα στο .myz (με ή χωρίς συμπίεση) και ενημερώνει το MyzNode
//      με το offset, το πραγματικό μέγεθος και αν είναι συμπιεσμένο.
// -------------------------------------------------------------------------------------------------------
void MyzArchive::processFile(const char* filePath, int fd, bool compress, off_t& dataOffset, Vector<MyzNode>& nodeList) {
    struct stat st;
    if (stat(filePath, &st) == -1) {
        std::perror("stat");
        return;
    }

    // 1) Φτιάχνουμε ένα MyzNode για το αρχείο
    MyzNode fileNode;
    std::strcpy(fileNode.path, filePath);
    fileNode.st_mode = st.st_mode;
    fileNode.uid = st.st_uid;
    fileNode.gid = st.st_gid;
    fileNode.inode = st.st_ino;
    fileNode.modTime = st.st_mtime;
    fileNode.accTime = st.st_atime;
    fileNode.creTime = st.st_ctime;
    fileNode.originalSize = st.st_size;

    // Έλεγχος αν το αρχείο έχει ήδη αποθηκευτεί (hard link)
    bool alreadyStored = false;
    for (size_t i = 0; i < nodeList.get_size(); ++i) {
        if (nodeList[i].inode == fileNode.inode) {
            // Το αρχείο έχει ήδη αποθηκευτεί, οπότε δεν ξαναγράφουμε τα δεδομένα.
            // Αντιγράφουμε τα offsets και τα μεγέθη από την πρώτη εγγραφή.
            fileNode.dataOffset = nodeList[i].dataOffset;
            fileNode.dataSize = nodeList[i].dataSize;
            fileNode.isCompressed = nodeList[i].isCompressed;
            alreadyStored = true;
            break;
        }
    }

    // Αν δεν έχει αποθηκευτεί ήδη, γράφουμε τα δεδομένα στο .myz
    if (!alreadyStored) {
        // 2. Εγγραφή δεδομένων στο .myz
        if (!compress) {
            // Χωρίς συμπίεση
            writeRawDataToMyz(fd, filePath, fileNode, dataOffset);
        } else {
            // Με gzip συμπίεση
            writeCompressedDataToMyz(fd, filePath, fileNode, dataOffset);
        }
    }

    // 3. Προσθέτουμε τον MyzNode στη λίστα μετά την εγγραφή των δεδομένων
    nodeList.push_back(fileNode);
}

// -------------------------------------------------------------------------------------------------------
// processSymbolicLink:
//   1) Χρησιμοποιεί lstat() για να διαβάσει πληροφορίες για τον symbolic link (όχι το target).
//   2) Συμπληρώνει τα πεδία του MyzNode (path, mode, UID, GID, timestamps κλπ.).
//   3) Διαβάζει το target του symlink με readlink() και το αποθηκεύει στο symlinkTarget.
//   4) Αν το target είναι σχετικό, το μετατρέπει σε πλήρες path (full path).
//   5) Δεν αποθηκεύει δεδομένα (dataOffset, dataSize = 0) αφού το symlink δεν έχει περιεχόμενο.
//   6) Προσθέτει τον MyzNode στη λίστα nodeList.
// -------------------------------------------------------------------------------------------------------
void MyzArchive::processSymbolicLink(const char* symbolicPath, Vector<MyzNode>& nodeList) {
    struct stat st;
    if (lstat(symbolicPath, &st) < 0) {
        perror("lstat");
        return;
    }
    
    // 1) Δημιουργούμε MyzNode για τον συμβολικό σύνδεσμο
    MyzNode symlinkNode;
    std::strcpy(symlinkNode.path, symbolicPath);
    symlinkNode.st_mode = st.st_mode;
    symlinkNode.uid = st.st_uid;
    symlinkNode.gid = st.st_gid;
    symlinkNode.inode = st.st_ino;
    symlinkNode.modTime = st.st_mtime;
    symlinkNode.accTime = st.st_atime;
    symlinkNode.creTime = st.st_ctime;

    // 2) Διαβάζουμε το target του symlink
    char targetBuffer[256];
    ssize_t len = readlink(symbolicPath, targetBuffer, sizeof(targetBuffer) - 1);
    if (len != -1) {
        targetBuffer[len] = '\0';
        
        // 3) Αν το target είναι σχετικό (δεν ξεκινάει με '/'), σχηματίζουμε το πλήρες path
        if (targetBuffer[0] != '/') {
            char dirPart[256];
            std::strcpy(dirPart, symbolicPath);
            char* lastSlash = std::strrchr(dirPart, '/');
            if (lastSlash) {
                *(lastSlash + 1) = '\0'; // κρατάμε το directory με trailing slash
                std::snprintf(symlinkNode.symlinkTarget, sizeof(symlinkNode.symlinkTarget),
                              "%s%s", dirPart, targetBuffer);
            } else {
                // Αν δεν υπάρχει slash, αποθηκεύουμε το target όπως είναι
                std::strncpy(symlinkNode.symlinkTarget, targetBuffer, sizeof(symlinkNode.symlinkTarget) - 1);
                symlinkNode.symlinkTarget[sizeof(symlinkNode.symlinkTarget) - 1] = '\0';
            }
        } else {
            // Αν είναι absolute path, αποθηκεύουμε το target όπως είναι
            std::strncpy(symlinkNode.symlinkTarget, targetBuffer, sizeof(symlinkNode.symlinkTarget) - 1);
            symlinkNode.symlinkTarget[sizeof(symlinkNode.symlinkTarget) - 1] = '\0';
        }
    } else {
        // Αν δεν μπορέσουμε να διαβάσουμε το symlink target, αφήνουμε κενή τη συμβολοσειρά
        perror("readlink");
        symlinkNode.symlinkTarget[0] = '\0';
    }

    // 4) Δεν αποθηκεύουμε δεδομένα για symlink, καθώς δείχνει σε άλλο αρχείο
    symlinkNode.dataOffset = 0;
    symlinkNode.dataSize = 0;
    symlinkNode.originalSize = 0;
    symlinkNode.isCompressed = false;

    // 5) Προσθέτουμε τον κόμβο του συμβολικού συνδέσμου στη λίστα
    nodeList.push_back(symlinkNode);
}

// ------------------------------------------------------------------------------------------------
// shouldExtract: 
//   1) Αν η λίστα αρχείων (files) είναι άδεια, επιστρέφει πάντα true (εξάγουμε όλα τα αρχεία).
//   2) Αν δεν είναι άδεια, ελέγχει αν το path υπάρχει μέσα στη λίστα (files).
// ------------------------------------------------------------------------------------------------
bool MyzArchive::shouldExtract(Vector<char*> files, const char* path) {
    if (files.get_size() == 0) {
        return true; // Εξάγουμε τα πάντα
    }
    // Ελέγχουμε αν το path εμφανίζεται στη λίστα
    for (size_t i = 0; i < files.get_size(); ++i) {
        if (strcmp(files[i], path) == 0) {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------------------------------------------
// extractDirectory: 
//   1) Δημιουργεί τοπικά έναν φάκελο με το node.path (mkdir).
//   2) Ρυθμίζει τα δικαιώματα του φακέλου (chmod) σύμφωνα με το node.st_mode.
// ------------------------------------------------------------------------------------------------
void MyzArchive::extractDirectory(const MyzNode& node) {
    // Αν ο φάκελος δεν υπάρχει, τον δημιουργούμε με 0755
    int ret = mkdir(node.path, 0755); 
    if (ret < 0 && errno != EEXIST) {
        perror("mkdir");
        std::cerr << "[WARN] Αποτυχία δημιουργίας φακέλου: " << node.path << "\n";
    } else {
        // Ρυθμίζουμε τα δικαιώματα (αγνοώντας τυχόν ειδικά bits)
        chmod(node.path, node.st_mode & 0777);
    }
}

// ------------------------------------------------------------------------------------------------
// extractFile: 
//   1) Μετακινείται στο offset (node.dataOffset) του .myz όπου ξεκινούν τα δεδομένα του αρχείου.
//   2) Καθορίζει το outputFilePath (αντιγράφοντας το node.path) και δημιουργεί τους ενδιάμεσους φακέλους.
//   3) Αν node.isCompressed == true, διαβάζει/αποσυμπιέζει με readCompressedDataToMyz. Αλλιώς, readRawDataToMyz.
//   4) Ρυθμίζει τα δικαιώματα του εξαγόμενου αρχείου σύμφωνα με το node.st_mode.
// ------------------------------------------------------------------------------------------------
void MyzArchive::extractFile(int fdArchive, const MyzNode& node) {
    // 1) Μετακινούμαστε στο offset όπου βρίσκονται τα δεδομένα
    if (lseek(fdArchive, node.dataOffset, SEEK_SET) < 0) {
        perror("lseek dataOffset");
        return;
    }
    
    // 2) Ετοιμάζουμε το τελικό path εξόδου
    char outputFilePath[512];
    std::snprintf(outputFilePath, sizeof(outputFilePath), "%s", node.path);

    // Δημιουργούμε τους ενδιάμεσους φακέλους (π.χ. dirA/dirB)
    makeAllDirsIfNeeded(outputFilePath);
    
    // 3) Διαβάζουμε τα δεδομένα είτε συμπιεσμένα είτε ωμά
    off_t localOffset = node.dataOffset;
    if (node.isCompressed) {
        readCompressedDataToMyz(fdArchive, outputFilePath, const_cast<MyzNode&>(node), localOffset);
    } else {
        readRawDataToMyz(fdArchive, outputFilePath, const_cast<MyzNode&>(node), localOffset);
    }
    
    // 4) Ρυθμίζουμε δικαιώματα σύμφωνα με το node.st_mode (μόνο τα βασικά bits)
    chmod(outputFilePath, node.st_mode & 0777);
}

// ------------------------------------------------------------------------------------------------
// extractSymbolicLink:
//   1) Δημιουργεί τυχόν ενδιάμεσους φακέλους για το node.path (με makeAllDirsIfNeeded).
//   2) Ελέγχει αν υπάρχει ήδη κάτι στο node.path:
//      - Αν είναι ήδη συμβολικός σύνδεσμος, δεν κάνει τίποτα.
//      - Αν είναι άλλο αρχείο/φάκελος, το διαγράφει (unlink).
//   3) Παίρνει το τρέχον working directory (basePath) με getcwd.
//   4) Σχηματίζει τις πλήρεις διαδρομές (fullSymlinkTarget, fullPath) συνδυάζοντας basePath με node.symlinkTarget / node.path.
//   5) Χρησιμοποιεί την κλήση symlink(fullSymlinkTarget, fullPath) για να δημιουργήσει το symbolic link.
// ------------------------------------------------------------------------------------------------
void MyzArchive::extractSymbolicLink(const MyzNode& node) {
    // 1) Δημιουργούμε τους φακέλους που χρειάζονται για το node.path
    makeAllDirsIfNeeded(node.path);

    // 2) Ελέγχουμε αν υπάρχει ήδη κάτι στο node.path
    struct stat sb;
    if (lstat(node.path, &sb) == 0) {
        if (S_ISLNK(sb.st_mode)) {
            return;
        } else {
            // Διαγράφουμε οποιοδήποτε μη-symlink, αν υπάρχει
            if (unlink(node.path) < 0) {
                perror("unlink");
                std::cerr << "[ERROR] Αποτυχία διαγραφής: " << node.path << "\n";
                return;
            }
        }
    }

    // 3) Παίρνουμε το τρέχον working directory σε buffer basePath
    char basePath[PATH_MAX];
    if (!getcwd(basePath, sizeof(basePath))) {
        perror("getcwd");
        std::cerr << "[ERROR] Αδυναμία εύρεσης του current working directory.\n";
        return;
    }

    // Βεβαιωνόμαστε ότι το basePath τελειώνει σε '/'
    size_t baseLen = std::strlen(basePath);
    if (baseLen > 0 && basePath[baseLen - 1] != '/') {
        if (baseLen + 1 < sizeof(basePath)) {
            basePath[baseLen] = '/';
            basePath[baseLen + 1] = '\0';
        }
    }

    // 4) Φτιάχνουμε τις πλήρεις διαδρομές (fullSymlinkTarget και fullPath)
    // Για το symlink target
    char fullSymlinkTarget[PATH_MAX * 2];
    if (std::strncmp(node.symlinkTarget, basePath, std::strlen(basePath)) == 0) {
        // Αν το node.symlinkTarget ξεκινά ήδη με basePath
        std::strncpy(fullSymlinkTarget, node.symlinkTarget, sizeof(fullSymlinkTarget) - 1);
        fullSymlinkTarget[sizeof(fullSymlinkTarget) - 1] = '\0';
    } else {
        // Προσθέτουμε το basePath μπροστά
        std::strncpy(fullSymlinkTarget, basePath, sizeof(fullSymlinkTarget) - 1);
        fullSymlinkTarget[sizeof(fullSymlinkTarget) - 1] = '\0';
        std::strncat(fullSymlinkTarget, node.symlinkTarget,
                     sizeof(fullSymlinkTarget) - std::strlen(fullSymlinkTarget) - 1);
    }

    // Για το node.path
    char fullPath[PATH_MAX * 2];
    if (std::strncmp(node.path, basePath, std::strlen(basePath)) == 0) {
        // Αν το node.path ξεκινά ήδη με basePath
        std::strncpy(fullPath, node.path, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';
    } else {
        // Εναλλακτικά, προσθέτουμε το basePath
        std::strncpy(fullPath, basePath, sizeof(fullPath) - 1);
        fullPath[sizeof(fullPath) - 1] = '\0';
        std::strncat(fullPath, node.path,
                     sizeof(fullPath) - std::strlen(fullPath) - 1);
    }

    // 5) Δημιουργούμε το symbolic link: symlink(<target>, <linkpath>)
    if (symlink(fullSymlinkTarget, fullPath) < 0) {
        perror("symlink");
        std::cerr << "[ERROR] Αποτυχία δημιουργίας symbolic link: " << fullPath
                  << " -> " << fullSymlinkTarget << "\n";
        return;
    }
}

// ------------------------------------------------------------------------------------------------
// makeAllDirsIfNeeded:
//   1) Αποκόπτει το τελευταίο τμήμα της διαδρομής (το αρχείο) για να βρει τους ενδιάμεσους φακέλους.
//   2) Αναλύει τμηματικά (με strtok) τους φακέλους.
//   3) Δημιουργεί κάθε φάκελο στην πορεία, αν δεν υπάρχει ήδη (errno == EEXIST).
// ------------------------------------------------------------------------------------------------
void MyzArchive::makeAllDirsIfNeeded(const char* outPath) {
    // 1) Κάνουμε αντίγραφο του outPath, γιατί θα το τροποποιήσουμε με strtok().
    char temp[512];
    std::strncpy(temp, outPath, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';

    // Εντοπίζουμε την τελευταία κάθετο
    char* p = std::strrchr(temp, '/');
    if (!p) {
        // Αν δεν υπάρχει '/', τότε δεν απαιτούνται ενδιάμεσοι φάκελοι
        return;
    }

    // "Αποκόπτουμε" το τελικό τμήμα (π.χ. όνομα αρχείου), αφήνοντας μόνο τη διαδρομή των φακέλων
    *p = '\0';

    // 2) Αναλύουμε τμηματικά τη διαδρομή και δημιουργούμε κάθε φάκελο σταδιακά
    char dirs[512];
    std::strncpy(dirs, temp, sizeof(dirs));
    dirs[sizeof(dirs) - 1] = '\0';

    char* token = std::strtok(dirs, "/");

    // Χρησιμοποιούμε πίνακα C-style για να "χτίζουμε" σταδιακά τη διαδρομή
    char pathSoFar[512];
    pathSoFar[0] = '\0'; // ξεκινάμε με κενό

    while (token) {
        // Αν το pathSoFar δεν είναι κενό, προσθέτουμε '/'
        if (pathSoFar[0] != '\0') {
            std::strncat(pathSoFar, "/", sizeof(pathSoFar) - std::strlen(pathSoFar) - 1);
        }
        // Προσθέτουμε το τρέχον τμήμα (token)
        std::strncat(pathSoFar, token, sizeof(pathSoFar) - std::strlen(pathSoFar) - 1);

        // 3) Δημιουργούμε αυτόν τον φάκελο αν δεν υπάρχει ήδη
        int ret = mkdir(pathSoFar, 0755);
        if (ret < 0 && errno != EEXIST) {
            perror("mkdir intermediate");
            // Μπορείς να σταματήσεις εδώ ή να συνεχίσεις, ανάλογα με την πολιτική σου
        }

        // Προχωράμε στον επόμενο "φάκελο"
        token = std::strtok(nullptr, "/");
    }
}
