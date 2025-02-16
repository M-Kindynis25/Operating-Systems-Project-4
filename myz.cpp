/* myz.cpp */
#include <iostream> // για debug εκτυπώσεις
#include <cstring>  // strcmp
#include "MyzArchive.hpp"
#include "vector.hpp"

void printUsage(const char* progName);

int main(int argc, char* argv[]) {
    // Έλεγχος ελάχιστων ορισμάτων
    if (argc < 3) {
        printUsage(argv[0]);
        return 1;
    }

    // 1ο όρισμα: σημαία (flag)
    char* flag = argv[1];
    // 2ο όρισμα: όνομα του .myz αρχείου
    char* archiveName;
    bool compress = false; 
    if (strcmp(argv[2], "-j") == 0){
        if (argc < 4) {
            std::cerr << "Λείπει το όνομα του .myz αρχείου μετά τη σημαία -j.\n";
            printUsage(argv[0]);
            return 1;
        }
        compress = true;
        archiveName = argv[3];
    } else {
        archiveName = argv[2];
    }

    // Από το 3ο όρισμα και μετά: αρχεία/κατάλογοι
    Vector<char*> fileList;
    for (int i = 3; i < argc; ++i) {
        if (compress && i == 3) continue;
        fileList.push_back(argv[i]);
    }
    // Δημιουργούμε ένα αντικείμενο MyzArchive και καλούμε createArchive
    MyzArchive myz(archiveName, fileList, compress);

    // Διαχείριση της σημαίας:
    if (strcmp(flag, "-c") == 0) {
        // Δημιουργία νέου .myz
        myz.createArchive();
    }
    else if (strcmp(flag, "-a") == 0) {
        // Προσθήκη σε υπάρχον .myz
        myz.addToArchive();
    }
    else if (strcmp(flag, "-x") == 0 && !compress) {
        // Εξαγωγή
        myz.extractArchive();
    }
    else if (strcmp(flag, "-d") == 0 && !compress) {
        // Διαγραφή
        myz.deleteFromArchive();
    }
    else if (strcmp(flag, "-m") == 0 && !compress) {
        // Εμφάνιση μεταδεδομένων
        myz.printMetadata();
    }
    else if (strcmp(flag, "-p") == 0 && !compress) {
        // Εκτύπωση ιεραρχίας
        myz.printHierarchy();
    }
    else if (strcmp(flag, "-q") == 0 && !compress) {
        // Ερώτηση ύπαρξης αρχείων
        myz.queryArchive();
    }
    else {
        std::cerr << "Μη έγκυρη σημαία: " << flag << "\n";
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}

void printUsage(const char* progName) {
    std::cout << "Χρήση: " << progName << " <σημαία> [-j] <αρχείο.myz> [αρχεία/κατάλογοι...]\n";
    std::cout << "\n";
    std::cout << "Περιγραφή:\n";
    std::cout << "  Το πρόγραμμα χειρίζεται αρχεία αρχείου τύπου '.myz'. Υποστηρίζει δημιουργία,\n";
    std::cout << "  προσθήκη, εξαγωγή, διαγραφή, ερώτηση μεταδεδομένων, εκτύπωση ιεραρχίας και ερώτηση\n";
    std::cout << "  ύπαρξης αρχείων.\n";
    std::cout << "\n";
    std::cout << "Επιλογές:\n";
    std::cout << "  -c <αρχείο.myz> [αρχεία/κατάλογοι...]   Δημιουργεί νέο αρχείο '.myz'.\n";
    std::cout << "  -a <αρχείο.myz> [αρχεία/κατάλογοι...]   Προσθέτει αρχεία/καταλόγους σε υπάρχον αρχείο '.myz'.\n";
    std::cout << "  -x <αρχείο.myz>                         Εξάγει όλα τα περιεχόμενα από το '.myz'.\n";
    std::cout << "  -d <αρχείο.myz> [αρχεία/κατάλογοι...]   Διαγράφει αρχεία/καταλόγους από το '.myz'.\n";
    std::cout << "  -m <αρχείο.myz>                         Εμφανίζει τα μεταδεδομένα του '.myz'.\n";
    std::cout << "  -p <αρχείο.myz>                         Εκτυπώνει την ιεραρχία των περιεχομένων.\n";
    std::cout << "  -q <αρχείο.myz> [αρχεία...]             Ελέγχει αν τα αρχεία υπάρχουν στο '.myz'.\n";
    std::cout << "\n";
    std::cout << "Σημαία -j (προαιρετική):\n";
    std::cout << "  Η σημαία '-j' ενεργοποιεί τη συμπίεση κατά τη δημιουργία ή την προσθήκη αρχείων.\n";
    std::cout << "  Πρέπει να τοποθετείται αμέσως μετά τη σημαία ενέργειας (π.χ. '-c', '-a').\n";
    std::cout << "\n";
}
