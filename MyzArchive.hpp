/* MyzArchive.hpp */
#ifndef MYZ_ARCHIVE_HPP
#define MYZ_ARCHIVE_HPP

#include <cstring>
#include "vector.hpp"

struct MyzHeader {
    char magic[4];        // π.χ. "MYZ!"
    uint32_t version;     // π.χ. 1
    uint64_t dataOffset;  // Από ποιο byte ξεκινάει η Data Area;
    uint64_t metaOffset;  // Από ποιο byte ξεκινάει η περιοχή Metadata;
    uint64_t metaCount;   // Πόσα "MyzNode" θα βρεις;

    // Constructor-Βοηθητικό για αρχικοποίηση
    MyzHeader() : magic{'M', 'Y', 'Z', '!'}, version(1), dataOffset(0), metaOffset(0), metaCount(0) {}
};

struct MyzNode {
    char path[256];     // Πλήρες όνομα αρχείου/καταλόγου (π.χ. "dirA/subdir/file.txt")
    mode_t st_mode;     // Από stat() -> αν είναι directory, file κ.λπ.
    off_t dataOffset;   // Σε ποιο offset του .myz αρχίζει αυτό το αρχείο
    off_t dataSize;     // Πόσα bytes αποθηκεύτηκαν (συμπιεσμένα ή μη)
    off_t originalSize; // Προαιρετικά, πόσα bytes ήταν πριν τη συμπίεση
    bool isCompressed;  // True αν το αρχείο είναι σε μορφή gzip

    // Επιπλέον πεδία για Links:
    ino_t inode;            // Inode number (για έλεγχο hard links)
    uid_t uid;              // ID ιδιοκτήτη
    gid_t gid;              // ID ομάδας
    char symlinkTarget[512]; // Target path για symbolic links
    
    // Ενδεικτικές χρονοσφραγίδες
    time_t modTime;   // τελευταία τροποποίηση
    time_t accTime;   // τελευταία πρόσβαση
    time_t creTime;   // χρόνος δημιουργίας

    MyzNode() : st_mode(0), dataOffset(0), dataSize(0), originalSize(0), isCompressed(false),
                inode(0), uid(0), gid(0), modTime(0), accTime(0), creTime(0) {
        std::memset(path, 0, sizeof(path));
        std::memset(symlinkTarget, 0, sizeof(symlinkTarget));
    }
};

class MyzArchive {
public:
    // Κατασκευαστής και Destructor
    MyzArchive(char* in_archiveName, Vector<char*>& in_files, bool in_compress);
    ~MyzArchive();

    // Βασικές λειτουργίες: δημιουργία, προσθήκη, εξαγωγή, διαγραφή, εμφάνιση μεταδεδομένων, ιεραρχίας και query
    void createArchive();
    void addToArchive();
    void extractArchive();
    void deleteFromArchive();
    void printMetadata();
    void printHierarchy();
    void queryArchive();

private:
    // Στοιχεία του αρχείου .myz
    char* archiveName;           // Όνομα αρχείου .myz
    Vector<char*> files;         // Λίστα αρχείων/φακέλων προς επεξεργασία
    bool compress;               // Flag για συμπίεση (gzip)
    MyzHeader header;            // Header του αρχείου .myz
    Vector<MyzNode> nodeList;    // Λίστα με τα metadata (MyzNodes)

    // Βοηθητικές συναρτήσεις εγγραφής/ανάγνωσης
    void writeHeaderToArchive();     // Γράφει το header στο offset 0
    void writeNodeListToArchive();   // Γράφει το nodeList στο offset header.metaOffset
    bool readArchive();              // Διαβάζει το header και το nodeList από το .myz
    void modeToString(mode_t mode, char* str);
    void printHeader(const MyzHeader& header);
    void printNode(const MyzNode& node);

    // Επεξεργασία εισόδων (αρχεία, φάκελοι, symbolic links)
    bool processOne(const char* path, int fd, bool compress, off_t& dataOffset, Vector<MyzNode>& nodeList);
    void processDirectory(const char* dirPath, int fd, bool compress, off_t& dataOffset, Vector<MyzNode>& nodeList);
    void processFile(const char* filePath, int fd, bool compress, off_t& dataOffset, Vector<MyzNode>& nodeList);
    void processSymbolicLink(const char* path, Vector<MyzNode>& nodeList);

    // Λειτουργίες εξαγωγής
    bool shouldExtract(Vector<char*> files, const char* path);
    void extractDirectory(const MyzNode& node);
    void extractFile(int fdArchive, const MyzNode& node);
    void extractSymbolicLink(const MyzNode& node);
    void makeAllDirsIfNeeded(const char* outPath);
    void renameOrCopyFile(const char* src, const char* dst);

    // Βοηθητικές συναρτήσεις για ανάγνωση/εγγραφή δεδομένων
    void writeRawDataToMyz(int myzFd, const char* filePath, MyzNode& node, off_t& dataOffset);
    void writeCompressedDataToMyz(int myzFd, const char* filePath, MyzNode& node, off_t& dataOffset);
    void readRawDataToMyz(int myzFd, const char* filePath, MyzNode& node, off_t& dataOffset);
    void readCompressedDataToMyz(int myzFd, const char* filePath, MyzNode& node, off_t& dataOffset);
};


#endif // MYZ_ARCHIVE_HPP
