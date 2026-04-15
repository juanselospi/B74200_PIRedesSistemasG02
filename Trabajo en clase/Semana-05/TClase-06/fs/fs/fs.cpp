#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace std;


class FileSystem {
private:
    json fs;

    bool hasBlock(int blockId) const {
        return fs.contains("blocks") && 
        fs["blocks"].contains(to_string(blockId));
    }

    const json& getBlock(int blockId) const {
        if (!hasBlock(blockId)) {
            throw runtime_error("Bloque no encontrado: " + to_string(blockId));
        }
        return fs["blocks"][to_string(blockId)];
    }

    int getRootDirectoryBlock() const {
        if (!fs.contains("superblock") || 
        !fs["superblock"].contains("root_directory")) {
            throw runtime_error("El superbloque no contiene root_directory");
        }
        return fs["superblock"]["root_directory"].get<int>();
    }

    vector<json> getDirectoryEntries() const {
        vector<json> entries;
        int current = getRootDirectoryBlock();

        while (true) {
            const json& block = getBlock(current);

            if (!block.contains("type") || block["type"] != "directory") {
                throw runtime_error("El bloque " + to_string(current) +
                 " no es un directorio");
            }

            if (block.contains("entries") && block["entries"].is_array()) {
                for (const auto& entry : block["entries"]) {
                    entries.push_back(entry);
                }
            }

            if (!block.contains("next") || block["next"].is_null()) {
                break;
            }

            current = block["next"].get<int>();
        }

        return entries;
    }

    int findInodeByName(const string& figureName) const {
        const auto entries = getDirectoryEntries();

        for (const auto& entry : entries) {
            if (entry.contains("name") && entry["name"].get<string>() ==
             figureName) {
                if (!entry.contains("inode")) {
                    throw runtime_error("La entrada de directorio no contiene inode para: " + figureName);
                }
                return entry["inode"].get<int>();
            }
        }

        return -1;
    }

    vector<string> readSegmentPieces(int segmentId) const {
        vector<string> pieces;
        int current = segmentId;

        while (true) {
            const json& block = getBlock(current);

            if (!block.contains("type") || block["type"] != "segment") {
                throw runtime_error("El bloque " + to_string(current) + 
                " no es un segmento");
            }

            if (block.contains("pieces") && block["pieces"].is_array()) {
                for (const auto& piece : block["pieces"]) {
                    pieces.push_back(piece.get<string>());
                }
            }

            if (!block.contains("next") || block["next"].is_null()) {
                break;
            }

            current = block["next"].get<int>();
        }

        return pieces;
    }

public:
    explicit FileSystem(const string& filePath) {
        ifstream file(filePath);
        if (!file.is_open()) {
            throw runtime_error("No se pudo abrir el archivo JSON: " 
                + filePath);
        }

        file >> fs;
    }

    void validateBasicStructure() const {
        if (!fs.contains("superblock")) {
            throw runtime_error("Falta el superbloque");
        }
        if (!fs.contains("blocks")) {
            throw runtime_error("Falta la sección de bloques");
        }
        if (!fs["blocks"].is_object()) {
            throw runtime_error("La sección blocks debe ser un objeto JSON");
        }

        const json& super = fs["superblock"];
        if (!super.contains("block_size") || 
        super["block_size"].get<int>() != 256) {
            throw runtime_error("El tamaño lógico de bloque debe ser 256 bytes");
        }

        int root = getRootDirectoryBlock();
        const json& rootBlock = getBlock(root);
        if (!rootBlock.contains("type") || rootBlock["type"] != "directory") {
            throw runtime_error("El root_directory no apunta a un bloque tipo directory");
        }
    }

    void ls() const {
        const auto entries = getDirectoryEntries();

        if (entries.empty()) {
            cout << "Directorio vacío\n";
            return;
        }

        for (const auto& entry : entries) {
            string name = entry.value("name", "<sin_nombre>");
            int inode = entry.value("inode", -1);
            cout << name << "\t(inode " << inode << ")\n";
        }
    }

    void cat(const string& figureName) const {
        int inodeId = findInodeByName(figureName);
        if (inodeId == -1) {
            cout << "Figura no encontrada: " << figureName << "\n";
            return;
        }

        const json& inode = getBlock(inodeId);
        if (!inode.contains("type") || inode["type"] != "inode") {
            throw runtime_error("El bloque " + to_string(inodeId) + 
            " no es un inode");
        }

        cout << "Figura: " << inode.value("figure_name", figureName) << "\n";

        if (inode.contains("metadata") && inode["metadata"].is_object()) {
            cout << "Metadatos:\n";
            for (auto it = inode["metadata"].begin(); it 
            != inode["metadata"].end(); ++it) {
                cout << "  - " << it.key() << ": " << it.value() << "\n";
            }
        }

        cout << "Piezas:\n";

        if (!inode.contains("segments") || !inode["segments"].is_array()) {
            cout << "  (sin segmentos)\n";
            return;
        }

        int count = 1;
        for (const auto& seg : inode["segments"]) {
            int segmentId = seg.get<int>();
            vector<string> pieces = readSegmentPieces(segmentId);
            for (const auto& piece : pieces) {
                cout << "  " << count++ << ". " << piece << "\n";
            }
        }
    }
};

void printUsage(const string& programName) {
    cerr << "Uso:\n";
    cerr << "  " << programName << " <archivo_json> ls\n";
    cerr << "  " << programName << " <archivo_json> cat <nombre_figura>\n";
}

int main(int argc, char* argv[]) {
    try {
        if (argc < 3) {
            printUsage(argv[0]);
            return 1;
        }

        string jsonFile = argv[1];
        string command = argv[2];

        FileSystem fs(jsonFile);
        fs.validateBasicStructure();

        if (command == "ls") {
            fs.ls();
        } else if (command == "cat") {
            if (argc < 4) {
                cerr << "Falta el nombre de la figura para cat\n";
                printUsage(argv[0]);
                return 1;
            }
            fs.cat(argv[3]);
        } else {
            cerr << "Comando no soportado: " << command << "\n";
            printUsage(argv[0]);
            return 1;
        }

        return 0;
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}

