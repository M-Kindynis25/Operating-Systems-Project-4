/* ReadWriteData.hpp */
#include "MyzArchive.hpp"
#include "vector.hpp"
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>

// -------------------------------------------------------------------------------------------------------
// writeRawDataToMyz:
//   1) Ανοίγει το αρχείο που θέλουμε να αρχειοθετήσουμε (σε μορφή raw, χωρίς συμπίεση).
//   2) Αντιγράφει όλα τα bytes στο αρχείο .myz, ενημερώνοντας παράλληλα το offset.
//   3) Συμπληρώνει στο MyzNode τις πληροφορίες dataOffset, dataSize, originalSize (ίδιο με dataSize),
//      και θέτει isCompressed = false.
// -------------------------------------------------------------------------------------------------------
void MyzArchive::writeRawDataToMyz(int myzFd, const char* filePath, MyzNode& node, off_t& dataOffset) {
    int inFd = open(filePath, O_RDONLY);
    if (inFd < 0) {
        perror("open");
        return;
    }

    node.dataOffset = dataOffset;

    const size_t BUFSIZE = 4096;
    char buf[BUFSIZE];
    ssize_t r;
    while ((r = read(inFd, buf, BUFSIZE)) > 0) {
        write(myzFd, buf, r);
        dataOffset += r;
    }

    node.dataSize = dataOffset - node.dataOffset;
    node.originalSize = node.dataSize;
    node.isCompressed = false;

    close(inFd);
}

// -------------------------------------------------------------------------------------------------------
// writeCompressedDataToMyz:
//   1) Δημιουργεί ένα προσωρινό αρχείο, όπου θα γράψει τα συμπιεσμένα δεδομένα μέσω gzip.
//   2) Εκτελεί την εντολή "gzip -c <filePath> > <tempFile>".
//   3) Ανοίγει το συμπιεσμένο αρχείο και αντιγράφει τα bytes στο .myz.
//   4) Ενημερώνει στο MyzNode το dataOffset, dataSize και θέτει isCompressed = true.
//   5) Στο τέλος διαγράφει το προσωρινό αρχείο.
// -------------------------------------------------------------------------------------------------------
void MyzArchive::writeCompressedDataToMyz(int myzFd, const char* filePath, MyzNode& node, off_t& dataOffset) {
    // 1. Φτιάχνουμε προσωρινό αρχείο για το gzip
    char tempCompressed[] = "/tmp/myz_compressXXXXXX";
    int tempFd = mkstemp(tempCompressed);
    if (tempFd < 0) {
        perror("mkstemp");
        return;
    }
    close(tempFd);

    // 2. Χρησιμοποιούμε system για συμπίεση
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gzip -c '%s' > '%s'", filePath, tempCompressed);
    int ret = system(cmd);
    if (ret != 0) {
        std::cerr << "Σφάλμα κατά τη συμπίεση με gzip.\n";
        unlink(tempCompressed);
        return;
    }

    // 3. Ανοίγουμε το συμπιεσμένο αρχείο και το γράφουμε στο .myz
    int inFd = open(tempCompressed, O_RDONLY);
    if (inFd < 0) {
        perror("open tempCompressed");
        unlink(tempCompressed);
        return;
    }

    node.dataOffset = dataOffset;

    const size_t BUFSIZE = 4096;
    char buf[BUFSIZE];
    ssize_t r;
    while ((r = read(inFd, buf, BUFSIZE)) > 0) {
        write(myzFd, buf, r);
        dataOffset += r;
    }

    // 4. Ενημερώνει το MyzNode
    node.dataSize = dataOffset - node.dataOffset;
    node.isCompressed = true;

    close(inFd);
    // 5. Διαγράφουμε το προσωρινό αρχείο
    unlink(tempCompressed); 
}

// -------------------------------------------------------------------------------------------------------
// readRawDataToMyz:
//   1) Ανοίγει το αρχείο εξόδου (filePath) για εγγραφή.
//   2) Διαβάζει από το .myz (myzFd) node.dataSize bytes (ξεκινώντας από dataOffset).
//   3) Γράφει τα bytes ωμά (χωρίς αποσυμπίεση) στο αρχείο εξόδου.
//   4) Ενημερώνει το dataOffset καθώς διαβάζονται δεδομένα.
// -------------------------------------------------------------------------------------------------------
void MyzArchive::readRawDataToMyz(int myzFd, const char* filePath, MyzNode& node, off_t& dataOffset) {
    // 1) Ανοίγουμε το αρχείο εξόδου για εγγραφή (overwrite)
    int outFd = open(filePath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outFd < 0) {
        perror("open output file");
        return;
    }

    // 2) Ρυθμίζουμε πόσα bytes θα διαβάσουμε από το .myz
    off_t bytesToRead = node.dataSize;
    const size_t BUFSIZE = 4096;
    char buffer[BUFSIZE];

    // 3) Διαβάζουμε chunk-chunk από το .myz και γράφουμε στο αρχείο εξόδου
    while (bytesToRead > 0) {
        ssize_t toRead = (bytesToRead > (off_t)BUFSIZE) ? (ssize_t)BUFSIZE : (ssize_t)bytesToRead;
        ssize_t r = read(myzFd, buffer, toRead);
        if (r < 0) {
            perror("read from archive");
            close(outFd);
            return;
        }
        if (r == 0) {
            // EOF πριν διαβάσουμε όλα τα δεδομένα
            break;
        }
        ssize_t w = write(outFd, buffer, r);
        if (w < 0) {
            perror("write to output file");
            close(outFd);
            return;
        }
        bytesToRead -= r;
        dataOffset  += r;  // Ενημερώνουμε το dataOffset
    }

    // 4) Κλείνουμε το αρχείο εξόδου
    close(outFd);
}


// -------------------------------------------------------------------------------------------------------
// readCompressedDataToMyz:
//   1) Δημιουργεί ένα προσωρινό αρχείο (tmpCompressed) για να γράψει τα συμπιεσμένα δεδομένα.
//   2) Διαβάζει από το .myz (myzFd) node.dataSize bytes και τα αποθηκεύει στο tmpCompressed. 
//   3) Αποσυμπιέζει τα δεδομένα με χρήση gzip σε δεύτερο προσωρινό αρχείο (tmpDecompressed).
//   4) Μετακινεί ή αντιγράφει το αποσυμπιεσμένο αρχείο στο filePath.
//   5) Διαγράφει τα προσωρινά αρχεία για καθαριότητα.
// -------------------------------------------------------------------------------------------------------
void MyzArchive::readCompressedDataToMyz(int myzFd, const char* filePath, MyzNode& node, off_t& dataOffset) {
    // 1) Φτιάχνουμε προσωρινό αρχείο για τα συμπιεσμένα δεδομένα
    char tmpCompressed[] = "/tmp/myz_compressXXXXXX";
    int fdTmp = mkstemp(tmpCompressed);
    if (fdTmp < 0) {
        perror("mkstemp");
        return;
    }

    // 2) Διαβάζουμε από το .myz και γράφουμε στο tmpCompressed
    off_t bytesToRead = node.dataSize;
    const size_t BUFSIZE = 4096;
    char buffer[BUFSIZE];
    while (bytesToRead > 0) {
        ssize_t toRead = (bytesToRead > (off_t)BUFSIZE) ? (ssize_t)BUFSIZE : (ssize_t)bytesToRead;
        ssize_t r = read(myzFd, buffer, toRead);
        if (r < 0) {
            perror("read from archive");
            close(fdTmp);
            remove(tmpCompressed);
            return;
        }
        if (r == 0) {
            // EOF πριν διαβάσουμε όλα τα δεδομένα
            break;
        }
        if (write(fdTmp, buffer, r) < 0) {
            perror("write to tmpCompressed");
            close(fdTmp);
            remove(tmpCompressed);
            return;
        }
        bytesToRead -= r;
        dataOffset  += r;
    }
    close(fdTmp);

    // 3) Δημιουργούμε δεύτερο προσωρινό αρχείο για την αποσυμπίεση
    char tmpDecompressed[] = "/tmp/myz_decompressXXXXXX";
    int fdDec = mkstemp(tmpDecompressed);
    if (fdDec < 0) {
        perror("mkstemp decompress");
        remove(tmpCompressed);
        return;
    }
    close(fdDec);

    // Χρησιμοποιούμε gzip -d για να αποσυμπιέσουμε τα δεδομένα
    char cmd[1024];
    std::snprintf(cmd, sizeof(cmd), "gzip -d < '%s' > '%s'", tmpCompressed, tmpDecompressed);
    int ret = system(cmd);
    if (ret != 0) {
        std::cerr << "[WARN] Αποτυχία αποσυμπίεσης gzip για: " << node.path << "\n";
        remove(tmpCompressed);
        remove(tmpDecompressed);
        return;
    }

    // 4) Μετακινούμε ή αντιγράφουμε το αποσυμπιεσμένο αρχείο στο filePath
    renameOrCopyFile(tmpDecompressed, filePath);

    // 5) Καθαρίζουμε τα προσωρινά αρχεία
    remove(tmpCompressed);
    remove(tmpDecompressed);
}

// -------------------------------------------------------------------------------------------------------
// renameOrCopyFile:
//   1) Προσπαθεί να μετακινήσει το src -> dst με std::rename().
//   2) Αν αποτύχει και το σφάλμα είναι EXDEV (διαφορετικό filesystem), κάνει copy fallback.
//   3) Διαβάζει τμηματικά από το src και γράφει στο dst μέχρι να τελειώσουν τα δεδομένα.
// -------------------------------------------------------------------------------------------------------
void MyzArchive::renameOrCopyFile(const char* src, const char* dst) {
    if (std::rename(src, dst) == 0) {
        // Μετακίνηση επιτυχής, τέλος
        return;
    }
    if (errno != EXDEV) {
        // Αν δεν φταίει το cross-device link, εμφανίζουμε σφάλμα
        perror("rename");
        return;
    }
    // Αλλιώς διαβάζουμε από src και γράφουμε σε dst
    int inFd = open(src, O_RDONLY);
    if (inFd < 0) {
        perror("open src");
        return;
    }
    int outFd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (outFd < 0) {
        perror("open dst");
        close(inFd);
        return;
    }

    char buffer[4096];
    ssize_t r;
    while ((r = read(inFd, buffer, sizeof(buffer))) > 0) {
        write(outFd, buffer, r);
    }
    close(inFd);
    close(outFd);
}