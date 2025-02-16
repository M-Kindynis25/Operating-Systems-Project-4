# myz: Σύστημα Αρχειοθέτησης Λογικών Ιεραρχιών

## 1. Περιγραφή
Το πρόγραμμα **myz** είναι ένα εργαλείο αρχειοθέτησης που υλοποιεί λειτουργίες παρόμοιες με τα `zip`, `tar` ή `rar`. Σκοπός του είναι η αποθήκευση και ανάκτηση ιεραρχικών δομών (αρχεία, κατάλογοι και symbolic links) του συστήματος αρχείων του Linux, μαζί με τα απαραίτητα μεταδεδομένα (όπως permissions, timestamps, UID, GID, inode, κ.ά.). Επίσης, παρέχει τη δυνατότητα συμπίεσης των περιεχομένων με χρήση του `gzip`, μέσω της επιλογής `-j`.

Το πρόγραμμα υποστηρίζει τις παρακάτω λειτουργίες:
- **Δημιουργία αρχείου (-c):** Αρχικοποιεί ένα νέο αρχείο με επέκταση `.myz` αποθηκεύοντας τα δεδομένα και τα μεταδεδομένα των οντοτήτων που δίνονται ως είσοδος.
- **Προσθήκη (-a):** Προσθέτει επιπλέον αρχεία/καταλόγους σε ένα ήδη υπάρχον αρχείο `.myz` χωρίς να γίνεται πλήρης ανακατασκευή.
- **Εξαγωγή (-x):** Εξάγει όλα ή επιλεγμένα αρχεία/καταλόγους από το αρχείο `.myz` στον τρέχοντα κατάλογο εργασίας.
- **Διαγραφή (-d):** Διαγράφει συγκεκριμένες οντότητες από το αρχείο, ενημερώνοντας τα μεταδεδομένα.
- **Εμφάνιση μεταδεδομένων (-m):** Εμφανίζει πληροφορίες για όλες τις οντότητες που περιέχονται στο αρχείο.
- **Εκτύπωση ιεραρχίας (-p):** Εκτυπώνει την ιεραρχική δομή των αποθηκευμένων οντοτήτων.
- **Ερώτηση ύπαρξης (-q):** Ελέγχει εάν συγκεκριμένα αρχεία ή κατάλογοι υπάρχουν μέσα στο αρχείο.
- **Συμπίεση (-j):** Όταν ενεργοποιείται, τα αρχεία συμπιέζονται μέσω `gzip` πριν αποθηκευτούν.

---

## 2. Δομή του Αρχείου .myz
Το αρχείο αρχειοθέτησης έχει σχεδιαστεί ώστε να επιτρέπει γρήγορη πρόσβαση και ενημέρωση, χωρίζοντάς το σε τρία βασικά τμήματα:

1. **Επικεφαλίδα (Header):**  
   - Περιέχει το magic number (π.χ. "MYZ!"), την έκδοση του αρχείου, και δείκτες (offsets) για τις δύο επόμενες περιοχές.
   - Ορίζεται σε σταθερό μέγεθος (512 bytes) για ευκολία στην ανάγνωση/εγγραφή.

2. **Data Area:**  
   - Αποθηκεύονται τα δεδομένα των αρχείων. Αυτά μπορεί να είναι ωμά ή συμπιεσμένα (ανάλογα με την επιλογή `-j`).

3. **Metadata Area:**  
   - Περιέχει μια λίστα από εγγραφές `MyzNode`, κάθε μία αποθηκεύοντας πληροφορίες για μία οντότητα (πλήρες path, permissions, ιδιοκτήτη, ομάδα, inode, timestamps, κλπ.).
   - Η τοποθέτηση αυτών των εγγραφών καθορίζεται από τα offsets που αποθηκεύονται στην επικεφαλίδα.

---

## 3. Σχεδιαστικές Επιλογές και Αιτιολόγηση

### 3.1 Δομή Αρχείου και Οργάνωση Δεδομένων
- **Επικεφαλίδα & Metadata:**  
  Η χρήση μιας σταθερής επικεφαλίδας επιτρέπει την ταχεία ανάγνωση των βασικών πληροφοριών του αρχείου, ενώ η διαχωρισμένη περιοχή metadata (με την καταγραφή του offset και του αριθμού εγγραφών) εξασφαλίζει ότι οι ενημερώσεις (π.χ. μετά από προσθήκη ή διαγραφή) μπορούν να γίνουν χωρίς ανακατασκευή ολόκληρου του αρχείου.
  
- **MyzNode:**  
  Η δομή `MyzNode` αποθηκεύει όλα τα κρίσιμα μεταδεδομένα που απαιτούνται για την ακριβή αναπαράσταση της κατάστασης κάθε αρχείου/καταλόγου (συμπεριλαμβανομένων των symbolic links). Με αυτόν τον τρόπο, η επακόλουθη ανάκτηση και επαναφορά των αρχείων γίνεται αξιόπιστα και πλήρως διαμορφωμένη όπως στο αρχικό σύστημα.

### 3.2 Υποστήριξη Συμπίεσης
- **Ενεργοποίηση με τη Σημαία `-j`:**  
  Όταν ενεργοποιείται η σημαία `-j`, τα δεδομένα των αρχείων συμπιέζονται χρησιμοποιώντας το `gzip`. Η υλοποίηση βασίζεται σε προσωρινά αρχεία και κλήσεις στο σύστημα (`system()`) για την εκτέλεση της εντολής `gzip`. Κατά την εξαγωγή, γίνεται η αντίστροφη διαδικασία (αποσυμπίεση) ώστε το αρχικό περιεχόμενο να αποκαθίσταται.

### 3.3 Αναδρομική Επεξεργασία Καταλόγων και Διαχείριση Symbolic Links
- **Αναδρομική Επεξεργασία:**  
  Η συνάρτηση `processDirectory` καλεί αναδρομικά τη `processOne` για όλα τα περιεχόμενα ενός καταλόγου, εξασφαλίζοντας ότι ακόμη και οι πολύπλοκες ιεραρχίες αποθηκεύονται πλήρως.
  
- **Διαχείριση Symbolic Links:**  
  Αντί να αποθηκεύεται το περιεχόμενο του target ενός symbolic link, ο κώδικας καταγράφει το link και το target του, διατηρώντας έτσι την ακεραιότητα της αναπαράστασης χωρίς επιπρόσθετη εγγραφή περιεχομένων.

### 3.4 Αποδοτική Ενημέρωση του Αρχείου
- **Προσθήκη (-a) και Διαγραφή (-d):**  
  Οι λειτουργίες αυτές έχουν σχεδιαστεί ώστε να τροποποιούν μόνο τις απαραίτητες εγγραφές στο αρχείο, αποφεύγοντας την πλήρη ανακατασκευή του. Αυτό επιτυγχάνεται μέσω της ενημέρωσης του nodeList και της επαναγραφής του header, διατηρώντας έτσι την αποδοτικότητα και την ταχύτητα.
  
- **Επαναγραφογράφηση του Header:**  
  Μετά από κάθε αλλαγή, το header επαναγράφεται για να διασφαλιστεί ότι οι αλλαγές στο dataOffset, metaOffset και metaCount είναι συνεπείς.

### 3.5 Χρήση Δομών Δεδομένων
- **Vector:**  
  Για την αποθήκευση λιστών (π.χ. ονόματα αρχείων, MyzNodes) χρησιμοποιείται μια απλή δυναμική δομή `Vector`, η οποία παρέχει λειτουργίες push_back και get_size για ευέλικτη διαχείριση των δεδομένων.
  
- **TreeNode:**  
  Η δομή `TreeNode` χρησιμοποιείται για την αναπαράσταση και εκτύπωση της ιεραρχίας των καταλόγων, διευκολύνοντας την απεικόνιση της λογικής δομής με αναδρομικές κλήσεις.

### 3.6 Χειρισμός Σφαλμάτων και Δοκιμές
- **Έλεγχοι Εισόδου/Εξόδου:**  
  Ο κώδικας περιέχει εκτενείς ελέγχους (π.χ. lstat, open, read, write) ώστε σε περίπτωση αποτυχίας να εμφανίζονται κατανοητά μηνύματα σφάλματος.
  
- **Valgrind:**  
  Στο Makefile υπάρχουν ειδικά targets για έλεγχο διαρροών μνήμης μέσω του Valgrind, εξασφαλίζοντας ότι ο κώδικας είναι αξιόπιστος και χωρίς μνήμης διαρροές.
### 3.7 Διαχείριση Hard Links και Symbolic Links

Το **myz** υποστηρίζει όχι μόνο κανονικά αρχεία και καταλόγους, αλλά και **hard links** και **symbolic links**, με τον εξής τρόπο:

1. **Hard Links**  
   - Για τον εντοπισμό hard links, το πρόγραμμα ελέγχει το `inode` (π.χ. το πεδίο `inode` στο `MyzNode`) κάθε αποθηκευμένης οντότητας.  
   - Εάν δύο διαφορετικά paths στο σύστημα αρχείων αντιστοιχούν στο ίδιο `inode`, τότε θεωρούνται hard links και δεν αποθηκεύονται ξανά τα δεδομένα.  
   - Το **myz** κρατάει μόνο μία φορά το περιεχόμενο (data) για συγκεκριμένο `inode`, ενώ για τα υπόλοιπα hard links αποθηκεύει αναφορά στο ίδιο `dataOffset` και `dataSize`. Έτσι εξοικονομείται χώρος στο τελικό αρχείο `.myz`.

2. **Symbolic Links**  
   - Για τα symbolic links, δεν αποθηκεύεται το περιεχόμενο του target. Αντ’ αυτού, η δομή `MyzNode` περιέχει το `path` (για τον ίδιο τον σύνδεσμο) και το `symlinkTarget` (δηλαδή το πραγματικό αρχείο ή κατάλογο στο οποίο δείχνει ο σύνδεσμος).  
   - Εάν το target είναι σχετική διαδρομή, ο κώδικας επιχειρεί να το μετατρέψει σε πλήρη (absolute) για να διευκολύνει τη μετέπειτα εξαγωγή.  
   - Κατά την **εξαγωγή** του symbolic link, το **myz** δημιουργεί ξανά το σύνδεσμο στο κατάλληλο σημείο, χωρίς να αντιγράψει επιπλέον δεδομένα.

---

## 4. Δομή και Οργάνωση Κώδικα
Ο κώδικας του project έχει οργανωθεί σε πολλαπλά αρχεία για καλύτερη modularity και ευκολία συντήρησης. Ακολουθεί μια λεπτομερής περιγραφή κάθε αρχείου και της συνεισφοράς του:

- **myz.cpp:**  
  - Περιέχει το `main()`, το οποίο διαχειρίζεται τα ορίσματα της γραμμής εντολών.
  - Ελέγχει την εγκυρότητα των παραμέτρων και εμφανίζει οδηγίες χρήσης σε περίπτωση σφάλματος.
  - Δημιουργεί το αντικείμενο `MyzArchive` και δρομολογεί την εκτέλεση στην αντίστοιχη λειτουργία (δημιουργία, προσθήκη, εξαγωγή, διαγραφή, εμφάνιση μεταδεδομένων, εκτύπωση ιεραρχίας, ή ερώτηση ύπαρξης).

- **MyzArchive.hpp / MyzArchive.cpp:**
    - **Κύρια Λειτουργικότητα (σε μια πρόταση με απαρεμφατικές εντολές):**  
    Χρησιμοποιήστε την κλάση `MyzArchive` ως τον πυρήνα του προγράμματος για να δημιουργήσετε νέα αρχεία αρχειοθέτησης (createArchive), προσθέσετε δεδομένα σε υπάρχοντα αρχεία (addToArchive), εξάγετε την αρχική δομή (extractArchive), διαγράψετε επιλεγμένες εγγραφές (deleteFromArchive), εμφανίσετε μεταδεδομένα και ιεραρχία (printMetadata, printHierarchy) και ελέγξετε την ύπαρξη συγκεκριμένων οντοτήτων (queryArchive).

    - **Δομές Δεδομένων:**  
        - **MyzHeader:**  
            Αποθηκεύει τον «μαγικό» αριθμό (magic), την έκδοση (version) και τα βασικά offsets (dataOffset, metaOffset) μαζί με τον αριθμό των εγγραφών (metaCount).  
        - **MyzNode:**  
            Περιέχει στοιχεία όπως το πλήρες `path`, τις άδειες πρόσβασης (`st_mode`), τη θέση και το μέγεθος των δεδομένων (`dataOffset`, `dataSize`), πληροφορίες συμπίεσης (`isCompressed`), καθώς και πρόσθετα πεδία για hard links, symbolic links και χρονοσφραγίδες.  


    - **Βασικές Μέθοδοι της Κλάσης:**
        - **Δημόσιες Μέθοδοι:**
            - **`createArchive()`**  
            Δημιουργεί ένα νέο αρχείο `.myz`, γράφοντας αρχικά το header στο offset 0, αποθηκεύοντας αναδρομικά τα δεδομένα των paths εισόδου και καταγράφοντας τους σχετικούς κόμβους (MyzNodes). Στο τέλος, ενημερώνει ξανά το header και γράφει τη λίστα των μεταδεδομένων (nodeList).
            - **`addToArchive()`**  
            Διαβάζει το υπάρχον αρχείο (`readArchive()`), ανοίγει το `.myz` σε λειτουργία read/write, προσθέτει τα νέα αρχεία/καταλόγους στο τέλος της περιοχής δεδομένων και επικαιροποιεί τον πίνακα μεταδεδομένων (nodeList) καθώς και το header.
            - **`extractArchive()`**  
            Φορτώνει από το `.myz` το header και τις εγγραφές (nodeList), ανοίγει το αρχείο σε λειτουργία ανάγνωσης και, για κάθε κόμβο, εξάγει τα αποθηκευμένα δεδομένα (αρχεία, καταλόγους ή symbolic links) στην αρχική τους μορφή.
            - **`deleteFromArchive()`**  
            Χρησιμοποιεί τη `readArchive()` για να διαβάσει το header και το nodeList, αφαιρεί από τον πίνακα μεταδεδομένων όσα paths ζητήθηκαν να διαγραφούν και ξαναγράφει την ενημερωμένη λίστα (nodeList) και το header στο `.myz`.
            - **`printMetadata()`**, **`printHierarchy()`**, **`queryArchive()`**  
            Παρέχουν λειτουργίες για την εμφάνιση λεπτομερών πληροφοριών σχετικά με τα αποθηκευμένα αρχεία (μεταδεδομένα), την εκτύπωση της ιεραρχίας σε μορφή δέντρου ή τον έλεγχο της ύπαρξης συγκεκριμένων αρχείων/φακέλων στο αρχείο `.myz`.

        - **Ιδιωτικές Βοηθητικές Μέθοδοι:**
            - **Εγγραφή/Ανάγνωση:**  
                - `writeHeaderToArchive()`: Γράφει το header στο offset 0.  
                - `writeNodeListToArchive()`: Γράφει τη λίστα μεταδεδομένων (nodeList) στην περιοχή metadata.  
                - `readArchive()`: Διαβάζει το header και το nodeList από το `.myz` και ελέγχει την εγκυρότητά του.
            - **Μετατροπή & Εκτύπωση:**  
                - `modeToString()`: Μετατρέπει τις άδειες πρόσβασης (mode) σε συμβολική μορφή (π.χ. rwxr-xr-x).  
                - `printHeader()`, `printNode()`: Εκτυπώνουν τα περιεχόμενα του header και κάθε κόμβου (MyzNode) για σκοπούς ελέγχου και debugging.
            - **Επεξεργασία Εισόδων (αρχεία/κατάλογοι/links):**  
                - `processOne()`: Αναγνωρίζει τον τύπο της διαδρομής (φάκελος, αρχείο ή symbolic link) και καλεί την κατάλληλη συνάρτηση (`processDirectory()`, `processFile()`, κ.λπ.).  
                - `processDirectory()`, `processFile()`, `processSymbolicLink()`: Διαχειρίζονται αναδρομικά το περιεχόμενο των φακέλων, τα δεδομένα των αρχείων και τα symbolic links αντίστοιχα.
            - **Εξαγωγή & Διαχείριση Δομής:**  
                - `shouldExtract()`: Καθορίζει αν μια οντότητα πρέπει να εξαχθεί (ανάλογα με τα ορίσματα εισόδου).  
                - `extractDirectory()`, `extractFile()`, `extractSymbolicLink()`: Ανακτούν τα δεδομένα από το `.myz` και αναδημιουργούν την αρχική δομή στο σύστημα αρχείων.  
                - `makeAllDirsIfNeeded()`: Δημιουργεί τους απαραίτητους ενδιάμεσους καταλόγους πριν την εξαγωγή.  
                - `renameOrCopyFile()`: Διαχειρίζεται τη μεταφορά ή αντιγραφή αρχείων (π.χ. μετά από αποσυμπίεση).
            - **Συναρτήσεις Ανάγνωσης/Εγγραφής Δεδομένων:**  
                - `writeRawDataToMyz()`, `writeCompressedDataToMyz()`: Εγγράφουν δεδομένα (ωμά ή συμπιεσμένα) στο αρχείο `.myz`.  
                - `readRawDataToMyz()`, `readCompressedDataToMyz()`: Διαβάζουν αντίστοιχα τα δεδομένα από το αρχείο και (αν είναι συμπιεσμένα) τα αποσυμπιέζουν.

    - **Σχεδιαστική Προσέγγιση:**
        - Το αρχείο `.myz` χωρίζεται σε ένα σταθερό header (για τα βασικά offsets και τον έλεγχο εγκυρότητας), μια περιοχή δεδομένων (data area) και μια περιοχή μεταδεδομένων (metadata area).
        - Κάθε ξεχωριστή λειτουργία (π.χ. προσθήκη, εξαγωγή, διαγραφή) γίνεται μέσω εξειδικευμένων μεθόδων, κάνοντας τον κώδικα ευκολότερο στη συντήρηση.


- **MyzProcessing.hpp:**  
  - Περιέχει τις βοηθητικές συναρτήσεις που χρησιμοποιούνται από την κλάση `MyzArchive` για την επεξεργασία αρχείων και καταλόγων.
  - **Λειτουργίες που περιέχει:**
    - **processOne:** Ελέγχει τον τύπο της διαδρομής (φάκελος, αρχείο, symbolic link) και καλεί την αντίστοιχη συνάρτηση επεξεργασίας.
    - **processDirectory:** Δημιουργεί ένα `MyzNode` για έναν φάκελο και αναδρομικά επεξεργάζεται όλα τα περιεχόμενά του.
    - **processFile:** Διαβάζει τα μεταδεδομένα ενός αρχείου, ελέγχει για hard links, και γράφει τα δεδομένα (με ή χωρίς συμπίεση) στο αρχείο.
    - **processSymbolicLink:** Επεξεργάζεται symbolic links, αποθηκεύοντας το target τους με κατάλληλη διαχείριση σχετικών διαδρομών.
    - **shouldExtract:** Ελέγχει αν μια διαδρομή περιλαμβάνεται στη λίστα εξαγωγής (ή αν η λίστα είναι κενή, εξάγει όλα τα αρχεία).
    - **extractDirectory, extractFile, extractSymbolicLink:** Ανάλογες συναρτήσεις για την εξαγωγή των δεδομένων, δημιουργία ενδιάμεσων φακέλων και ρύθμιση δικαιωμάτων.
    - **makeAllDirsIfNeeded:** Δημιουργεί όλους τους ενδιάμεσους καταλόγους που απαιτούνται για την τοποθέτηση ενός αρχείου εξόδου.


- **ReadWriteData.hpp:**  
  - Περιέχει συναρτήσεις για την ανάγνωση και εγγραφή raw και συμπιεσμένων δεδομένων στο αρχείο.
  - Διαχειρίζεται τη μεταφορά δεδομένων μεταξύ των αρχείων εισόδου/εξόδου και του αρχείου αρχειοθέτησης, συμπεριλαμβανομένης της αποσυμπίεσης κατά την εξαγωγή.

- **TreeNode.hpp:**  
  - Παρέχει την υλοποίηση της κλάσης `TreeNode`.
  - Επιτρέπει την αναπαράσταση της ιεραρχικής δομής των καταλόγων με μια μορφή δέντρου.
  - Διευκολύνει την αναδρομική εκτύπωση της δομής σε αναγνώσιμη μορφή.

- **vector.hpp:**  
  - Υλοποιεί μια απλή δυναμική δομή (vector) για την αποθήκευση λιστών.
  - Χρησιμοποιείται για να αποθηκεύσει τις λίστες των ονομάτων αρχείων/καταλόγων και των αντικειμένων `MyzNode`.

- **Makefile:**  
  - Ορίζει τους κανόνες μεταγλώττισης για όλα τα αρχεία του project.
  - Περιέχει targets για:
    - **Κατασκευή (all):** Μεταγλώττιση όλων των πηγών και δημιουργία του εκτελέσιμου.
    - **Clean:** Αφαίρεση αντικειμένων και εκτελέσιμων.
    - **Run και Valgrind:** Δοκιμαστικές εκτελέσεις με προ-ορισμένες εντολές για έλεγχο λειτουργίας και διαρροών μνήμης.

---

## 5. Οδηγίες Κατασκευής (Build)
Για να μεταγλωττίσετε το πρόγραμμα, εκτελέστε:
```bash
make
