#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <cstring>
#include "./nlohmann/json.hpp"

using json = nlohmann::json;
using namespace nlohmann::literals;

using std::cerr;
using std::cout;
using std::distance;
using std::endl;
using std::find;
using std::fstream;
using std::getline;
using std::ifstream;
using std::ios;
using std::isdigit;
using std::ofstream;
using std::stof;
using std::stoi;
using std::strcmp;
using std::streampos;
using std::string;
using std::stringstream;
using std::to_string;
using std::vector;

bool LeerJSON(const string& filename, vector<string>& names, vector<string>& datatypes, vector<int>& datatypesizes, string& primarykey, vector<string>& secondarykey)
{
    ifstream file(filename);
    if (file.fail()) {
        cerr << "Error abriendo el archivo!! " << endl;
        return false;
    }
    else {
        stringstream buffer;
        buffer << file.rdbuf();
        json jsonData = json::parse(buffer);
        if (jsonData.contains("fields") && jsonData["fields"].is_array()) {
            for (const auto& field : jsonData["fields"]) {
                if (field.contains("name") and field.contains("type") and field.contains("length")) {
                    names.push_back(field["name"]);
                    if (field["type"] == "char" or field["type"] == "int" or field["type"] == "float") {
                        datatypes.push_back(field["type"]);
                    }
                    else {
                        cerr << "Tipo de dato no apoyado, debe de ser char, float o int" << endl;
                        return false;
                    }
                    int x = field["length"];
                    if (field["type"] == "char") {
                        datatypesizes.push_back(field["length"]);
                    }
                    else {
                        if (x % 4 == 0) {
                            datatypesizes.push_back(field["length"]);
                        }
                        else {
                            cerr << "El tamanio del field tiene que ser consistente con el tamanio del tipo de dato!" << endl;
                            return false;
                        }
                    }
                } // fin del if si contiene length y type y demas
                else {
                    cerr << "El archivo JSON proporcionado no esta en orden, no contiene ya sea size, type o name " << endl;
                    return false;
                }

            } // fin del for
        }     // fin del primer if
        else {
            cerr << "El archivo no contiene fields como array" << endl;
            return false;
        }
        if (!jsonData.contains("primary-key")) {
            cerr << "El archivo no tiene llave primaria,y me parece que tenerlo es un requerimiento" << endl;
            return false;
        }
        primarykey = jsonData["primary-key"];
        bool flag = false;
        for (size_t i = 0; i < names.size(); i++) {
            if (names[i] == primarykey) {
                flag = true;
                break;
            }
        } // fin del for
        if (!flag) {
            cerr << "La llave primaria no se encuentra en el archivo" << endl;
            return false;
        }
        if (jsonData.contains("secondary-key")) {
            json secondaryKeyJSON = jsonData["secondary-key"];
            if (secondaryKeyJSON.is_array()) {
                for (const auto& key : jsonData["secondary-key"]) {
                    auto it = find(names.begin(), names.end(), key);
                    if (it == names.end()) {
                        cerr << "El indice secundario no se encuentra en el archivo" << endl;
                        return false;
                    }
                    secondarykey.push_back(key);
                }

            } // fin del if si es array
            else {
                auto it = find(names.begin(), names.end(), jsonData["secondary-key"]);
                if (it == names.end()) {
                    cerr << "El indice secundario no se encuentra en el archivo" << endl;
                    return false;
                }
                secondarykey.push_back(jsonData["secondary-key"]);
            }
        } // fin del if si contiene clave secundaria
    }     // fin de else que si se puede abrir el archivo
    return true;
}
int byteoffset(const vector<string>& cosa)
{
    int acumulador = 0;
    for (size_t i = 0; i < cosa.size(); i++) {
        acumulador += cosa[i].length();
    }

    return acumulador;
}

struct Campo {
    char name[25];
    char type[15];
    int length;
    Campo()
    {
    }
    Campo(string name1, string type1, int length1)
    {
        strncpy(this->name, name1.c_str(), sizeof(this->name) - 1);
        strncpy(this->type, type1.c_str(), sizeof(this->type) - 1);
        this->name[sizeof(this->name) - 1] = '\0';
        this->type[sizeof(this->type) - 1] = '\0';
        this->length = length1;
    }
};
vector<string> LeerRecords(const string _record)
{
    vector<string> recordsSeparados;
    string acumuladorFields = "";
    size_t i = 0;
    while (i < _record.length()) {
        if (_record[i] == '"') {
            i++;
            string fieldEspecial = "";
            while (i < _record.length()) {
                if (_record[i] == '"') {
                    if (i + 1 < _record.length() && _record[i + 1] == ',') {
                        recordsSeparados.push_back(fieldEspecial);
                        i += 2;
                        break;
                    }
                    else if (i + 1 == _record.length()) {
                        recordsSeparados.push_back(fieldEspecial);
                        i++;
                        break;
                    }
                }
                fieldEspecial += _record[i];
                i++;
            }
        }
        else if (_record[i] == ',') {
            recordsSeparados.push_back(acumuladorFields);
            acumuladorFields = "";
            i++;
        }
        else {
            acumuladorFields += _record[i];
            i++;
        }
    }
    recordsSeparados.push_back(acumuladorFields);
    return recordsSeparados;
}
void CreateHeader(const string& filename, const vector<string>& namesObjects, vector<string>& datatypes, vector<int>& datatypesizes, string& primarykey, vector<string>& secondarykey)
{
    ofstream writeFile;
    writeFile.open(filename, ios::binary | ios::app);
    // escribo el tamano del header en bytes
    int x2 = 4 + 4 + sizeof(Campo) * namesObjects.size() + 25 + 4 + 25 * secondarykey.size() + 8;
    writeFile.write(reinterpret_cast<char*>(&x2), sizeof(int));
    // escribe el tamano del vector de campos
    x2 = namesObjects.size();
    cout << "El numero de campos " << x2 << "  ";
    writeFile.write(reinterpret_cast<char*>(&x2), sizeof(int));
    Campo* acumulador = new Campo[x2];
    for (int i = 0; i < x2; i++) {
        Campo x1(namesObjects[i], datatypes[i], datatypesizes[i]);
        acumulador[i] = x1;
    }
    writeFile.write(reinterpret_cast<char*>(acumulador), sizeof(Campo) * x2);

    // escribimos la llave primaria
    char llaveprimariatemp[25];
    strncpy(llaveprimariatemp, primarykey.c_str(), sizeof(llaveprimariatemp) - 1);
    llaveprimariatemp[sizeof(llaveprimariatemp) - 1] = '\0';
    writeFile.write(reinterpret_cast<char*>(llaveprimariatemp), sizeof(llaveprimariatemp));

    // el tamano de la secundarykey vector
    x2 = secondarykey.size();
    writeFile.write(reinterpret_cast<char*>(&x2), sizeof(int));
    // creamos la llave secundaria

    x2 = 0;
    // escribimos las llaves secundarias
    for (auto b : secondarykey) {
        char acumuladorsecondarykeys[25];
        strncpy(acumuladorsecondarykeys, b.c_str(), sizeof(acumuladorsecondarykeys) - 1);
        acumuladorsecondarykeys[sizeof(acumuladorsecondarykeys) - 1] = '\0';
        writeFile.write(reinterpret_cast<char*>(acumuladorsecondarykeys), sizeof(acumuladorsecondarykeys));
        x2++;
    }
    x2 = 0;

    // escribimos el offset del primer elemento del avail list
    long int offsetavail = -1;
    writeFile.write(reinterpret_cast<char*>(&offsetavail), sizeof(offsetavail));

    delete[] acumulador;
    // myfile.close();
    writeFile.close();
} // fin del metodo
void LeerHeader(string& filename, int& sizeheader, Campo& headerfields, char*& primarykey2, char*& secundarias, long int& offsetavail, int& sizedelobjeto, int& sizevectorcampos, int& sizevectorsecondary)
{

    fstream readFile;
    readFile.open("Details.dat", ios::in | ios::binary);
    // leo el size del header
    readFile.read(reinterpret_cast<char*>(&sizeheader), sizeof(int));

    // leo el size del vector de campos
    readFile.read(reinterpret_cast<char*>(&sizevectorcampos), sizeof(int));

    // leo el vector de campos
    headerfields = new Campo[sizevectorcampos];
    readFile.read(reinterpret_cast<char*>(&headerfields), sizeof(Campo) * sizevectorcampos);
    // obtengo el tamano del objeto
    for (size_t i = 0; i < sizevectorcampos; i++) {
        sizedelobjeto += headerfields[i].length;
    }

    // leo la primary key
    primarykey2 = new char[25];
    readFile.read(reinterpret_cast<char*>(primarykey2), 25);
    // leo el tamano del vector de llaves secundarias
    readFile.read(reinterpret_cast<char*>(&sizevectorsecondary), sizeof(int));
    // leo el vector de llaves secundarias
    secundarias = new char[sizevectorsecondary];
    for (size_t i = 0; i < sizevectorsecondary; i++) {
        secundarias[i] = new char[25];
        readFile.read(reinterpret_cast<char*>(secundarias[i]), 25);
    }
    // leo la location del primer elemento del avail list
    readFile.read(reinterpret_cast<char*>(&offsetavail), sizeof(long int));

    readFile.close();
}
bool Comparar(char* arreglo, string cadena)
{
    int x = 0;
    bool flag = true;
    while (x < cadena.length()) {
        if (arreglo[x] != cadena[x]) {
            return false;
        }
        x++;
    }
    return true;
}
bool EsInt(const string& cosa)
{
    for (size_t i = 0; i < cosa.length(); i++) {
        if (isdigit(static_cast<unsigned char>(cosa[i]))) {}
        else {
            if (i == 0 and cosa[i] == '-') {}
            else {
                return false;
            }
        }
    }

    return true;
}
bool EsFloat(const string& cosa)
{
    for (size_t i = 0; i < cosa.length(); i++) {
        if (isdigit(static_cast<unsigned char>(cosa[i]))) {}
        else {
            if (i == 0 and cosa[i] == '-') {}
            else {
                if (cosa[i] == '.' and i != 0 and i != cosa.length() - 1) {

                }
                else {
                    return false;
                }
            }
        }
    }

    return true;
}
bool EscribirArchivo(int& sizeheader, int& sizevectorcampos, Campo& headerfields, const string& filename, char& primarykey2)
{
    int recordskipped = 0;
    string acumular = "";
    ofstream writeFile;

    writeFile.open("Details.dat", ios::app | ios::binary);
    if (writeFile.fail()) {
        cerr << "Error abriendo el archivo para escribir en el" << endl;
        return false;
    } // fin del if dail
    writeFile.seekp(sizeheader, ios::beg);

    ifstream readFile;
    readFile.open("practica.txt", ios::in);
    if (readFile.fail()) {
        cerr << "Error abriendo el archivo para escribir los contenidos de este en el otro" << endl;
        return false;
    } // fin del if dail

    getline(readFile, acumular);
    vector<string> nombresfields = LeerRecords(acumular);

    if (sizevectorcampos != nombresfields.size()) {
        cerr << "La longitud de campos no coinciden" << endl;
        return false;
    }
    else {
        for (int i = 0; i < nombresfields.size(); i++) {

            if (!Comparar(headerfields[i].name, nombresfields[i])) {
                cerr << "Los nombres de campos no coinciden" << endl;
                return false;
            } // fin del if
        }     // fin del for

        while (getline(readFile, acumular)) {
            vector<string> acumuladorcampos = LeerRecords(acumular);
            bool flag = true;
            //    if (!flag){
            if (acumuladorcampos.size() != nombresfields.size()) {
                cerr << "Hay mas campos de los necesarios,o hay menos!" << endl;
                recordskipped++;

            } // fin del if
            else {
                for (size_t i = 0; i < sizevectorcampos; i++) {
                    bool flag2 = false;
                    if (acumuladorcampos[i].length() == 0) {
                        flag2 = true;
                        if (strcmp(headerfields[i].name, primarykey2) == 0) {
                            flag = false;
                            recordskipped++;
                            break;
                        }
                        else {
                            if (strcmp(headerfields[i].type, "char") == 0) {
                                char* valorescribirchar = new char[acumuladorcampos[i].length()];
                                for (size_t h = 0; h < headerfields[i].length; h++) {
                                    valorescribirchar[h] = '\0';
                                }// fin del for
                                writeFile.write(reinterpret_cast<char*>(valorescribirchar), headerfields[i].length);
                                delete[] valorescribirchar;
                            }// fin de si es char
                            else if (strcmp(headerfields[i].type, "int") == 0) {
                                int x = 0;
                                writeFile.write(reinterpret_cast<char*>(&x), headerfields[i].length);
                            }
                            else if (strcmp(headerfields[i].type, "float") == 0) {
                                float x = 0;
                                writeFile.write(reinterpret_cast<char*>(&x), headerfields[i].length);
                            }
                        }
                    }

                    // si es char lo que vamos a escribir
                    if (!flag2) {
                        if (strcmp(headerfields[i].type, "char") == 0) {
                            if (acumuladorcampos[i].length() > headerfields[i].length) {
                                recordskipped++;
                                flag = false;
                                break;
                            }
                        } // fin de si es char
                        else if (strcmp(headerfields[i].type, "int") == 0) {
                            flag = EsInt(acumuladorcampos[i]);
                            if (!flag) { recordskipped++; break; }
                        }
                        else if (strcmp(headerfields[i].type, "float") == 0) {
                            flag = EsFloat(acumuladorcampos[i]);
                            if (!flag) { recordskipped++; break; }

                        }
                    }
                } // fin del for para sacar bandera
                if (flag) {
                    for (size_t i = 0; i < sizevectorcampos; i++) {

                        if (strcmp(headerfields[i].type, "char") == 0) {
                            char* valorescribirchar = new char[acumuladorcampos[i].length()];
                            for (size_t h = 0; h < headerfields[i].length; h++) {
                                valorescribirchar[h] = '\0';
                            }
                            for (size_t j = 0; j < acumuladorcampos[i].length(); j++) {
                                valorescribirchar[j] = acumuladorcampos[i][j];
                            }
                            writeFile.write(reinterpret_cast<char*>(valorescribirchar), headerfields[i].length);
                            delete[] valorescribirchar;
                        } // fin del if strcmp char

                        else if (strcmp(headerfields[i].type, "int") == 0) {
                            int x = stoi(acumuladorcampos[i]);
                            writeFile.write(reinterpret_cast<char*>(&x), headerfields[i].length);
                        } // fin del if strcmp es int
                        else if (strcmp(headerfields[i].type, "float") == 0) {
                            float x = stof(acumuladorcampos[i]);
                            writeFile.write(reinterpret_cast<char*>(&x), headerfields[i].length);
                        } // fin del if strcmp es float

                    } // fin del for para escribir
                }     // fin del if flag
            }         // fin del else si la cantidad de campos es igual a la del header
        }// el while que lee todos los archivos
        //}

    } // fin del else que nos dice si el numero de campso es igual al numero de campos en el header
    readFile.close();
    writeFile.close();
    cout << "  El numero de records saltados: " << recordskipped;
    return true;
}
void LeerArchivo() {

}

int main(int argc, char* argv[])
{
    vector<string> namesObjects;
    vector<string> datatypes;
    vector<int> datatypesizes;
    string primarykey = "";
    vector<string> secondarykey;

    if (LeerJSON("JSON.txt", namesObjects, datatypes, datatypesizes, primarykey, secondarykey)) {

        CreateHeader("Details.dat", namesObjects, datatypes, datatypesizes, primarykey, secondarykey);
        // estos son los campos necesarios para leer el header
        int sizeheader = 0;
        Campo* headerfields;
        char* primarykey2;
        char** secundarias;
        long int offsetavail = 0;
        int sizedelobjeto = 0;
        string filename = "";
        int sizevectorcampos = 0;
        int sizevectorsecondary = 0;

        LeerHeader(filename, sizeheader, headerfields, primarykey2, secundarias, offsetavail, sizedelobjeto, sizevectorcampos, sizevectorsecondary);

        // estos son los campos necesarios para escribir el archivo y escribir en el int &sizeheader, int &sizevectorcampos, Campo *&headerfields
        if (EscribirArchivo(sizeheader, sizevectorcampos, headerfields, "as", primarykey2)) {
            cout << "siii!!" << endl;
        }
        // estos son los campos para leer el archivo binario
        string filename2 = "Details.dat";
        ifstream readFile;
        readFile.open(filename2, ios::binary);
        if (readFile.fail()) {
            cerr << "Error abriendo el archivo para lectura" << endl;
        }
        readFile.seekg(sizeheader, ios::beg);

        while (!readFile.eof()) {
            for (int i = 0; i < sizevectorcampos; i++) {
                char acumuladarx = 'a';
                readFile.read(reinterpret_cast<char*>(&acumuladarx), sizeof(char));
                //readFile.seekg(-1,ios::cur);
                if (acumuladarx == '*') {
                    cout << "penesaurio";
                    readFile.seekg(sizedelobjeto, ios::cur);
                }
                else {
                    //cout<<"asdasd"<<endl;
                    if (strcmp(headerfields[i].type, "char") == 0) {
                        char* acumulador2 = new char[headerfields[i].length];
                        for (int j = 0; j < headerfields[i].length; j++) {
                            acumulador2[j] = '\0';
                        }
                        readFile.read(reinterpret_cast<char*>(acumulador2), headerfields[i].length);

                        for (size_t h = 0; h < headerfields[i].length; h++) {
                            cout << acumulador2[h];
                        }
                        cout << "   ";
                        delete[] acumulador2;
                    } // fin del if si es char lo que vamos a leer
                    else if (strcmp(headerfields[i].type, "int") == 0) {
                        int acumuladorLeerint = 0;
                        readFile.read(reinterpret_cast<char*>(&acumuladorLeerint), headerfields[i].length);
                        cout << "  " << acumuladorLeerint << "  ";
                    } // fin del else if si lo que vamos a leer es int
                    else if (strcmp(headerfields[i].type, "float") == 0) {
                        float acumuladorLeerfloat = 0;
                        readFile.read(reinterpret_cast<char*>(&acumuladorLeerfloat), headerfields[i].length);
                        cout << " " << acumuladorLeerfloat << " ";
                    } // fin de lo que si vamoa a leer es float
                }     // fin del else si esta borrado el reguistro
            }
        } // fin del while para leer archivos
        readFile.close();

        for (int i = 0; i < sizevectorsecondary; i++) {
            delete[] secundarias[i];
        }
        delete[] secundarias;

    } // este es el if que nos dice si podemos leer el JSON

    return 0;
}
