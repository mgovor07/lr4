#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <limits>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <chrono>
#include <map>
#include <set>
#include <queue>

using namespace std;
namespace fs = filesystem;

// Перечисление для типов соединений
enum ConnectionType {
    STATION_TO_STATION,
    STATION_TO_PIPE,
    PIPE_TO_STATION,
    PIPE_TO_PIPE
};

// Оператор вывода для ConnectionType
ostream& operator<<(ostream& os, const ConnectionType& type) {
    switch (type) {
        case STATION_TO_STATION: os << "0"; break;
        case STATION_TO_PIPE: os << "1"; break;
        case PIPE_TO_STATION: os << "2"; break;
        case PIPE_TO_PIPE: os << "3"; break;
    }
    return os;
}

// Оператор ввода для ConnectionType
istream& operator>>(istream& is, ConnectionType& type) {
    int value;
    is >> value;
    switch (value) {
        case 0: type = STATION_TO_STATION; break;
        case 1: type = STATION_TO_PIPE; break;
        case 2: type = PIPE_TO_STATION; break;
        case 3: type = PIPE_TO_PIPE; break;
        default: type = STATION_TO_STATION; break;
    }
    return is;
}

struct Pipe {
    int id;
    string name;
    double length;
    int diameter;
    bool underRepair;
    bool inUse;  // используется ли в сети
    int startId;  // ID начальной точки (КС или трубы)
    int endId;   // ID конечной точки (КС или трубы)
    ConnectionType startType;  // тип начальной точки
    ConnectionType endType;    // тип конечной точки
};

struct CompressorStation {
    int id;
    string name;
    int totalWorkshops;
    int activeWorkshops;
    int stationClass;
};

// Структура для представления связи в сети
struct NetworkConnection {
    int pipeId;
    int startId;
    int endId;
    ConnectionType startType;
    ConnectionType endType;
};

// Структура для графа
struct GraphNode {
    int id;
    bool isStation;  // true - КС, false - труба
    vector<pair<int, int>> connections; // пары (id соседа, id трубы)
};

class Logger {
private:
    mutable ofstream logFile;
    
public:
    Logger() {
        logFile.open("pipeline_log.txt", ios::app);
        if (logFile.is_open()) {
            auto now = chrono::system_clock::now();
            auto time = chrono::system_clock::to_time_t(now);
            logFile << "\n=== Сессия начата: " << ctime(&time);
        }
    }
    
    ~Logger() {
        if (logFile.is_open()) {
            auto now = chrono::system_clock::now();
            auto time = chrono::system_clock::to_time_t(now);
            logFile << "=== Сессия завершена: " << ctime(&time) << endl;
            logFile.close();
        }
    }
    
    void log(const string& action, const string& details = "") const {
        if (logFile.is_open()) {
            auto now = chrono::system_clock::now();
            auto time = chrono::system_clock::to_time_t(now);
            
            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&time));
            
            logFile << timeStr << " | " << action;
            if (!details.empty()) {
                logFile << " | " << details;
            }
            logFile << endl;
        }
    }
};

class InputValidator {
public:
    static int getIntInput(const string& prompt, int min = numeric_limits<int>::min(),
                          int max = numeric_limits<int>::max()) {
        int value;
        while (true) {
            cout << prompt;
            string input;
            getline(cin, input);
            
            if (input.empty()) {
                cout << "Ошибка: ввод не может быть пустым.\n";
                continue;
            }
            
            try {
                value = stoi(input);
                if (value < min || value > max) {
                    cout << "Ошибка: значение должно быть от " << min << " до " << max << ".\n";
                    continue;
                }
                break;
            } catch (const exception&) {
                cout << "Ошибка: пожалуйста, введите целое число.\n";
            }
        }
        return value;
    }

    static double getDoubleInput(const string& prompt, double min = 0.0,
                               double max = numeric_limits<double>::max()) {
        double value;
        while (true) {
            cout << prompt;
            string input;
            getline(cin, input);
            
            if (input.empty()) {
                cout << "Ошибка: ввод не может быть пустым.\n";
                continue;
            }
            
            try {
                value = stod(input);
                if (value < min || value > max) {
                    cout << "Ошибка: значение должно быть от " << min << " до " << max << ".\n";
                    continue;
                }
                break;
            } catch (const exception&) {
                cout << "Ошибка: пожалуйста, введите число.\n";
            }
        }
        return value;
    }

    static string getStringInput(const string& prompt) {
        string input;
        while (true) {
            cout << prompt;
            getline(cin, input);
            if (!input.empty()) {
                break;
            }
            cout << "Ошибка: ввод не может быть пустым.\n";
        }
        return input;
    }

    static int getDiameterInput(const string& prompt) {
        vector<int> allowedDiameters = {500, 700, 1000, 1400};
        while (true) {
            cout << prompt << " (500, 700, 1000, 1400 мм): ";
            string input;
            getline(cin, input);
            
            if (input.empty()) {
                cout << "Ошибка: ввод не может быть пустым.\n";
                continue;
            }
            
            try {
                int diameter = stoi(input);
                for (int d : allowedDiameters) {
                    if (d == diameter) {
                        return diameter;
                    }
                }
                cout << "Ошибка: допустимые диаметры: 500, 700, 1000, 1400 мм\n";
            } catch (const exception&) {
                cout << "Ошибка: пожалуйста, введите целое число.\n";
            }
        }
    }
};

class PipelineSystem {
private:
    vector<Pipe> pipes;
    vector<CompressorStation> stations;
    vector<NetworkConnection> network;
    int nextPipeId = 1;
    int nextStationId = 1;
    Logger logger;

    int findPipeIndexById(int id) const {
        auto it = find_if(pipes.begin(), pipes.end(),
                         [id](const Pipe& p) { return p.id == id; });
        return it != pipes.end() ? distance(pipes.begin(), it) : -1;
    }

    int findStationIndexById(int id) const {
        auto it = find_if(stations.begin(), stations.end(),
                         [id](const CompressorStation& s) { return s.id == id; });
        return it != stations.end() ? distance(stations.begin(), it) : -1;
    }

    vector<int> parseIndicesFromInput(const string& input, const vector<int>& validIds) const {
        if (input == "all" || input == "ALL") {
            vector<int> allIndices;
            for (int i = 0; i < validIds.size(); ++i) {
                allIndices.push_back(i);
            }
            return allIndices;
        }
        
        vector<int> indices;
        stringstream ss(input);
        string token;
        
        while (getline(ss, token, ',')) {
            try {
                int id = stoi(token);
                auto it = find(validIds.begin(), validIds.end(), id);
                if (it != validIds.end()) {
                    indices.push_back(distance(validIds.begin(), it));
                } else {
                    cout << "Предупреждение: ID " << id << " не существует.\n";
                }
            } catch (const exception&) {
                cout << "Предупреждение: '" << token << "' не является числом.\n";
            }
        }
        
        sort(indices.begin(), indices.end());
        indices.erase(unique(indices.begin(), indices.end()), indices.end());
        return indices;
    }

    vector<int> selectMultipleObjects(const vector<int>& validIds, const string& objectType) const {
        if (validIds.empty()) {
            cout << "Нет доступных " << objectType << "!\n";
            return {};
        }
        
        cout << "\nВыберите ID " << objectType << " через запятую или 'all' для всех: ";
        string input;
        getline(cin, input);
        
        return parseIndicesFromInput(input, validIds);
    }

    vector<int> getPipeIds() const {
        vector<int> ids;
        for (const auto& pipe : pipes) {
            ids.push_back(pipe.id);
        }
        return ids;
    }

    vector<int> getStationIds() const {
        vector<int> ids;
        for (const auto& station : stations) {
            ids.push_back(station.id);
        }
        return ids;
    }

    static string toLower(const string& str) {
        string result = str;
        transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

    double calculateInactivePercent(const CompressorStation& station) const {
        return station.totalWorkshops > 0 ?
               100.0 * (station.totalWorkshops - station.activeWorkshops) / station.totalWorkshops : 0.0;
    }

    vector<int> findPipesByName(const string& searchName) const {
        vector<int> result;
        string searchLower = toLower(searchName);
        
        for (size_t i = 0; i < pipes.size(); ++i) {
            if (toLower(pipes[i].name).find(searchLower) != string::npos) {
                result.push_back(i);
            }
        }
        return result;
    }

    vector<int> findPipesByRepairStatus(bool repairStatus) const {
        vector<int> result;
        for (size_t i = 0; i < pipes.size(); ++i) {
            if (pipes[i].underRepair == repairStatus) {
                result.push_back(i);
            }
        }
        return result;
    }

    vector<int> findStationsByName(const string& searchName) const {
        vector<int> result;
        string searchLower = toLower(searchName);
        
        for (size_t i = 0; i < stations.size(); ++i) {
            if (toLower(stations[i].name).find(searchLower) != string::npos) {
                result.push_back(i);
            }
        }
        return result;
    }

    vector<int> findStationsByInactivePercent(double targetPercent, int comparisonType) const {
        vector<int> result;
        for (size_t i = 0; i < stations.size(); ++i) {
            double inactivePercent = calculateInactivePercent(stations[i]);
            bool match = false;
            
            switch (comparisonType) {
                case 1: match = (inactivePercent > targetPercent); break;
                case 2: match = (inactivePercent < targetPercent); break;
                case 3: match = (abs(inactivePercent - targetPercent) < 0.01); break;
            }
            
            if (match) {
                result.push_back(i);
            }
        }
        return result;
    }

    void displayObjects(const vector<int>& pipeIndices, const vector<int>& stationIndices) const {
        if (pipeIndices.empty() && stationIndices.empty()) {
            cout << "Нет объектов для отображения.\n";
            return;
        }
        
        if (!pipeIndices.empty()) {
            cout << "\nТрубы (" << pipeIndices.size() << ")\n";
            cout << "ID | Название | Длина | Диаметр | В ремонте | В сети | Начало -> Конец\n";
            cout << string(80, '-') << endl;
            for (int index : pipeIndices) {
                const Pipe& pipe = pipes[index];
                cout << setw(3) << pipe.id << " | "
                     << setw(10) << left << (pipe.name.length() > 10 ? pipe.name.substr(0, 7) + "..." : pipe.name) << " | "
                     << setw(6) << fixed << setprecision(2) << pipe.length << " | "
                     << setw(7) << pipe.diameter << " | "
                     << setw(10) << (pipe.underRepair ? "Да" : "Нет") << " | "
                     << setw(6) << (pipe.inUse ? "Да" : "Нет") << " | ";
                
                if (pipe.inUse) {
                    string startStr, endStr;
                    
                    if (pipe.startType == STATION_TO_STATION || pipe.startType == STATION_TO_PIPE) {
                        startStr = "КС" + to_string(pipe.startId);
                    } else {
                        startStr = "Тр" + to_string(pipe.startId);
                    }
                    
                    if (pipe.endType == STATION_TO_STATION || pipe.endType == PIPE_TO_STATION) {
                        endStr = "КС" + to_string(pipe.endId);
                    } else {
                        endStr = "Тр" + to_string(pipe.endId);
                    }
                    
                    cout << startStr << " -> " << endStr;
                } else {
                    cout << "Не подключена";
                }
                cout << endl;
            }
        }

        if (!stationIndices.empty()) {
            cout << "\nКС (" << stationIndices.size() << ")\n";
            cout << "ID | Название | Всего цехов | Работает | Незадействовано | Класс\n";
            cout << string(70, '-') << endl;
            for (int index : stationIndices) {
                const CompressorStation& station = stations[index];
                double inactivePercent = calculateInactivePercent(station);
                cout << setw(3) << station.id << " | "
                     << setw(10) << left << (station.name.length() > 10 ? station.name.substr(0, 7) + "..." : station.name) << " | "
                     << setw(12) << station.totalWorkshops << " | "
                     << setw(9) << station.activeWorkshops << " | "
                     << setw(15) << fixed << setprecision(1) << inactivePercent << "% | "
                     << station.stationClass << endl;
            }
        }
    }

    // Поиск свободной трубы по диаметру
    int findAvailablePipeByDiameter(int diameter) const {
        for (size_t i = 0; i < pipes.size(); ++i) {
            if (pipes[i].diameter == diameter && !pipes[i].inUse && !pipes[i].underRepair) {
                return i;
            }
        }
        return -1;
    }

    // Получение типа объекта по ID
    pair<bool, int> getObjectInfo(int id) const {
        // Проверяем, является ли ID станцией
        int stationIndex = findStationIndexById(id);
        if (stationIndex != -1) {
            return {true, stationIndex}; // true = станция
        }
        
        // Проверяем, является ли ID трубой
        int pipeIndex = findPipeIndexById(id);
        if (pipeIndex != -1) {
            return {false, pipeIndex}; // false = труба
        }
        
        return {false, -1}; // не найдено
    }

    // Определение типа соединения
    ConnectionType determineConnectionType(bool isStartStation, bool isEndStation) {
        if (isStartStation && isEndStation) return STATION_TO_STATION;
        if (!isStartStation && !isEndStation) return PIPE_TO_PIPE;
        if (isStartStation && !isEndStation) return STATION_TO_PIPE;
        return PIPE_TO_STATION;
    }

    // Проверка возможности соединения
    bool canConnectObjects(int startId, int endId, int diameter) {
        if (startId == endId) {
            cout << "Ошибка: нельзя соединить объект с самим собой!\n";
            return false;
        }
        
        // Получаем информацию об объектах
        auto [isStartStation, startIndex] = getObjectInfo(startId);
        auto [isEndStation, endIndex] = getObjectInfo(endId);
        
        if (startIndex == -1) {
            cout << "Ошибка: объект с ID " << startId << " не существует!\n";
            return false;
        }
        
        if (endIndex == -1) {
            cout << "Ошибка: объект с ID " << endId << " не существует!\n";
            return false;
        }
        
        // Проверка для труб
        if (!isStartStation) {
            const Pipe& startPipe = pipes[startIndex];
            if (startPipe.underRepair) {
                cout << "Ошибка: труба " << startId << " в ремонте!\n";
                return false;
            }
        }
        
        if (!isEndStation) {
            const Pipe& endPipe = pipes[endIndex];
            if (endPipe.underRepair) {
                cout << "Ошибка: труба " << endId << " в ремонте!\n";
                return false;
            }
        }
        
        // Проверка на существующее соединение (в одну сторону)
        for (const auto& conn : network) {
            if (conn.startId == startId && conn.endId == endId) {
                cout << "Ошибка: соединение между этими объектами уже существует!\n";
                return false;
            }
        }
        
        // Проверка диаметра для соединения труб с трубами
        if (!isStartStation && !isEndStation) {
            const Pipe& startPipe = pipes[startIndex];
            const Pipe& endPipe = pipes[endIndex];
            
            if (startPipe.diameter != diameter || endPipe.diameter != diameter) {
                cout << "Ошибка: диаметр соединяющей трубы должен совпадать с диаметром соединяемых труб!\n";
                cout << "Диаметр трубы " << startId << ": " << startPipe.diameter << " мм\n";
                cout << "Диаметр трубы " << endId << ": " << endPipe.diameter << " мм\n";
                cout << "Диаметр соединяющей трубы: " << diameter << " мм\n";
                return false;
            }
        }
        
        return true;
    }

    void connectObjects() {
        if (pipes.empty() && stations.empty()) {
            cout << "Нет объектов для соединения!\n";
            return;
        }
        
        viewAll();
        
        cout << "\nТипы соединений:\n";
        cout << "1. КС -> КС\n";
        cout << "2. КС -> Труба\n";
        cout << "3. Труба -> КС\n";
        cout << "4. Труба -> Труба\n";
        
        int connectionType = InputValidator::getIntInput("Выберите тип соединения: ", 1, 4);
        
        int startId, endId;
        string startPrompt, endPrompt;
        
        switch (connectionType) {
            case 1: // КС -> КС
                startPrompt = "Введите ID КС входа: ";
                endPrompt = "Введите ID КС выхода: ";
                break;
            case 2: // КС -> Труба
                startPrompt = "Введите ID КС входа: ";
                endPrompt = "Введите ID трубы выхода: ";
                break;
            case 3: // Труба -> КС
                startPrompt = "Введите ID трубы входа: ";
                endPrompt = "Введите ID КС выхода: ";
                break;
            case 4: // Труба -> Труба
                startPrompt = "Введите ID трубы входа: ";
                endPrompt = "Введите ID трубы выхода: ";
                break;
        }
        
        startId = InputValidator::getIntInput(startPrompt, 1);
        endId = InputValidator::getIntInput(endPrompt, 1);
        
        int diameter = InputValidator::getDiameterInput("Введите диаметр соединяющей трубы");
        
        // Проверка возможности соединения
        if (!canConnectObjects(startId, endId, diameter)) {
            return;
        }
        
        // Определение типов объектов
        auto [isStartStation, startIndex] = getObjectInfo(startId);
        auto [isEndStation, endIndex] = getObjectInfo(endId);
        
        // Поиск доступной трубы
        int pipeIndex = findAvailablePipeByDiameter(diameter);
        
        if (pipeIndex != -1) {
            // Используем существующую трубу
            pipes[pipeIndex].inUse = true;
            pipes[pipeIndex].startId = startId;
            pipes[pipeIndex].endId = endId;
            pipes[pipeIndex].startType = determineConnectionType(isStartStation, isEndStation);
            pipes[pipeIndex].endType = pipes[pipeIndex].startType; // для простоты
            
            NetworkConnection conn;
            conn.pipeId = pipes[pipeIndex].id;
            conn.startId = startId;
            conn.endId = endId;
            conn.startType = determineConnectionType(isStartStation, isEndStation);
            conn.endType = conn.startType;
            network.push_back(conn);
            
            string startTypeStr = isStartStation ? "КС" : "Труба";
            string endTypeStr = isEndStation ? "КС" : "Труба";
            
            cout << "Соединение создано: " << startTypeStr << " " << startId
                 << " -> " << endTypeStr << " " << endId
                 << " (труба ID: " << pipes[pipeIndex].id << ")\n";
            
            logger.log("Создано соединение",
                      startTypeStr + " " + to_string(startId) + " -> " +
                      endTypeStr + " " + to_string(endId) +
                      ", Труба ID: " + to_string(pipes[pipeIndex].id));
        } else {
            // Создаем новую трубу
            cout << "Свободной трубы диаметром " << diameter << " мм не найдено.\n";
            cout << "Создание новой трубы для соединения...\n";
            
            Pipe newPipe;
            newPipe.id = nextPipeId++;
            newPipe.name = InputValidator::getStringInput("Введите название соединяющей трубы: ");
            newPipe.length = InputValidator::getDoubleInput("Введите длину соединяющей трубы (км): ", 0.001);
            newPipe.diameter = diameter;
            newPipe.underRepair = false;
            newPipe.inUse = true;
            newPipe.startId = startId;
            newPipe.endId = endId;
            newPipe.startType = determineConnectionType(isStartStation, isEndStation);
            newPipe.endType = newPipe.startType;
            
            pipes.push_back(newPipe);
            
            NetworkConnection conn;
            conn.pipeId = newPipe.id;
            conn.startId = startId;
            conn.endId = endId;
            conn.startType = determineConnectionType(isStartStation, isEndStation);
            conn.endType = conn.startType;
            network.push_back(conn);
            
            string startTypeStr = isStartStation ? "КС" : "Труба";
            string endTypeStr = isEndStation ? "КС" : "Труба";
            
            cout << "Создана и соединена новая труба ID: " << newPipe.id << "\n";
            cout << "Соединение: " << startTypeStr << " " << startId
                 << " -> " << endTypeStr << " " << endId << "\n";
            
            logger.log("Создание и соединение новой трубы",
                      "Труба ID: " + to_string(newPipe.id) + ", " + newPipe.name +
                      ", " + startTypeStr + " " + to_string(startId) +
                      " -> " + endTypeStr + " " + to_string(endId));
        }
    }

    void disconnectPipe() {
        if (network.empty()) {
            cout << "Нет соединений в сети!\n";
            return;
        }
        
        viewNetwork();
        
        int pipeId = InputValidator::getIntInput("Введите ID трубы для разъединения: ", 1);
        int pipeIndex = findPipeIndexById(pipeId);
        
        if (pipeIndex == -1) {
            cout << "Труба с ID " << pipeId << " не найдена!\n";
            return;
        }
        
        if (!pipes[pipeIndex].inUse) {
            cout << "Труба не используется в сети!\n";
            return;
        }
        
        // Удаляем из сети
        auto it = remove_if(network.begin(), network.end(),
                           [pipeId](const NetworkConnection& conn) { return conn.pipeId == pipeId; });
        network.erase(it, network.end());
        
        // Сбрасываем флаг использования в трубе
        pipes[pipeIndex].inUse = false;
        pipes[pipeIndex].startId = 0;
        pipes[pipeIndex].endId = 0;
        
        cout << "Труба ID: " << pipeId << " отключена от сети.\n";
        logger.log("Отключение трубы от сети", "Труба ID: " + to_string(pipeId));
    }

    // Построение графа сети
    map<int, GraphNode> buildGraph() const {
        map<int, GraphNode> graph;
        
        // Добавляем станции
        for (const auto& station : stations) {
            GraphNode node;
            node.id = station.id;
            node.isStation = true;
            graph[station.id] = node;
        }
        
        // Добавляем трубы, которые являются узлами при соединении труб
        for (const auto& pipe : pipes) {
            if (pipe.inUse) {
                // Если труба соединена с другой трубой, она становится узлом
                auto [isStartStation, startIdx] = getObjectInfo(pipe.startId);
                auto [isEndStation, endIdx] = getObjectInfo(pipe.endId);
                
                if (!isStartStation || !isEndStation) {
                    // Добавляем трубу как узел графа
                    GraphNode node;
                    node.id = pipe.id;
                    node.isStation = false;
                    if (graph.find(pipe.id) == graph.end()) {
                        graph[pipe.id] = node;
                    }
                }
            }
        }
        
        // Добавляем соединения
        for (const auto& conn : network) {
            int pipeId = conn.pipeId;
            
            // Добавляем связь от начального узла к конечному через трубу
            if (graph.find(conn.startId) != graph.end()) {
                graph[conn.startId].connections.push_back({conn.endId, pipeId});
            }
            
            // Для неориентированного графа добавляем обратную связь
            // (раскомментировать если сеть неориентированная)
            // if (graph.find(conn.endId) != graph.end()) {
            //     graph[conn.endId].connections.push_back({conn.startId, pipeId});
            // }
        }
        
        return graph;
    }

    void viewNetwork() const {
        if (network.empty()) {
            cout << "Газотранспортная сеть пуста.\n";
            return;
        }
        
        cout << "\nГазотранспортная сеть (" << network.size() << " соединений)\n";
        cout << "Труба | Диаметр | Длина | Начало -> Конец | Тип соединения | Статус\n";
        cout << string(90, '-') << endl;
        
        for (const auto& conn : network) {
            int pipeIndex = findPipeIndexById(conn.pipeId);
            if (pipeIndex != -1) {
                const Pipe& pipe = pipes[pipeIndex];
                
                string startStr, endStr;
                if (conn.startType == STATION_TO_STATION || conn.startType == STATION_TO_PIPE) {
                    startStr = "КС" + to_string(conn.startId);
                } else {
                    startStr = "Тр" + to_string(conn.startId);
                }
                
                if (conn.endType == STATION_TO_STATION || conn.endType == PIPE_TO_STATION) {
                    endStr = "КС" + to_string(conn.endId);
                } else {
                    endStr = "Тр" + to_string(conn.endId);
                }
                
                string connTypeStr;
                switch (conn.startType) {
                    case STATION_TO_STATION: connTypeStr = "КС-КС"; break;
                    case STATION_TO_PIPE: connTypeStr = "КС-Труба"; break;
                    case PIPE_TO_STATION: connTypeStr = "Труба-КС"; break;
                    case PIPE_TO_PIPE: connTypeStr = "Труба-Труба"; break;
                }
                
                cout << setw(5) << pipe.id << " | "
                     << setw(7) << pipe.diameter << " | "
                     << setw(6) << fixed << setprecision(2) << pipe.length << " | "
                     << setw(5) << startStr << " -> "
                     << setw(9) << endStr << " | "
                     << setw(13) << connTypeStr << " | "
                     << (pipe.underRepair ? "В ремонте" : "Работает") << endl;
            }
        }
        
        // Статистика
        cout << "\nСтатистика сети:\n";
        cout << "Всего соединений: " << network.size() << endl;
        
        set<int> connectedStations;
        set<int> connectedPipes;
        for (const auto& conn : network) {
            auto [isStartStation, startIdx] = getObjectInfo(conn.startId);
            auto [isEndStation, endIdx] = getObjectInfo(conn.endId);
            
            if (isStartStation) {
                connectedStations.insert(conn.startId);
            } else {
                connectedPipes.insert(conn.startId);
            }
            
            if (isEndStation) {
                connectedStations.insert(conn.endId);
            } else {
                connectedPipes.insert(conn.endId);
            }
        }
        
        cout << "Подключенных КС: " << connectedStations.size() << " из " << stations.size() << endl;
        cout << "Подключенных труб: " << connectedPipes.size() << " из " << pipes.size() << endl;
        
        // Построение и вывод графа
        auto graph = buildGraph();
        if (!graph.empty()) {
            cout << "\nСтруктура сети (граф):\n";
            for (const auto& [nodeId, node] : graph) {
                string nodeType = node.isStation ? "КС" : "Труба";
                cout << nodeType << " " << nodeId << " соединен с: ";
                
                if (node.connections.empty()) {
                    cout << "ни с чем";
                } else {
                    for (size_t i = 0; i < node.connections.size(); ++i) {
                        auto [neighborId, pipeId] = node.connections[i];
                        auto [isNeighborStation, neighborIdx] = getObjectInfo(neighborId);
                        string neighborType = isNeighborStation ? "КС" : "Труба";
                        
                        cout << neighborType << " " << neighborId << " (через трубу " << pipeId << ")";
                        if (i < node.connections.size() - 1) {
                            cout << ", ";
                        }
                    }
                }
                cout << endl;
            }
        }
    }

    void topologicalSort() const {
        if (network.empty()) {
            cout << "Сеть пуста, сортировка невозможна.\n";
            return;
        }
        
        // Построение графа только для КС
        map<int, vector<int>> adjList;
        map<int, int> inDegree;
        
        // Инициализация степеней входа для всех станций
        for (const auto& station : stations) {
            inDegree[station.id] = 0;
        }
        
        // Построение списка смежности и подсчет степеней входа
        // Учитываем только соединения между станциями
        for (const auto& conn : network) {
            auto [isStartStation, startIdx] = getObjectInfo(conn.startId);
            auto [isEndStation, endIdx] = getObjectInfo(conn.endId);
            
            if (isStartStation && isEndStation) {
                adjList[conn.startId].push_back(conn.endId);
                inDegree[conn.endId]++;
            }
        }
        
        // Алгоритм Кана
        vector<int> result;
        vector<int> zeroDegreeNodes;
        
        for (const auto& [node, degree] : inDegree) {
            if (degree == 0) {
                zeroDegreeNodes.push_back(node);
            }
        }
        
        while (!zeroDegreeNodes.empty()) {
            int node = zeroDegreeNodes.back();
            zeroDegreeNodes.pop_back();
            result.push_back(node);
            
            for (int neighbor : adjList[node]) {
                inDegree[neighbor]--;
                if (inDegree[neighbor] == 0) {
                    zeroDegreeNodes.push_back(neighbor);
                }
            }
        }
        
        // Проверка на циклы
        if (result.size() != stations.size()) {
            cout << "Обнаружен цикл в сети КС! Сортировка невозможна.\n";
            
            // Поиск циклических зависимостей
            set<int> sortedStations(result.begin(), result.end());
            cout << "КС, образующие циклы: ";
            bool first = true;
            for (const auto& station : stations) {
                if (sortedStations.find(station.id) == sortedStations.end()) {
                    if (!first) cout << ", ";
                    cout << station.id << " (" << station.name << ")";
                    first = false;
                }
            }
            cout << endl;
            return;
        }
        
        // Вывод результата
        cout << "\nТопологическая сортировка КС:\n";
        for (size_t i = 0; i < result.size(); ++i) {
            int stationIndex = findStationIndexById(result[i]);
            if (stationIndex != -1) {
                cout << (i + 1) << ". КС ID: " << result[i]
                     << " (" << stations[stationIndex].name << ")\n";
            }
        }
    }

    // Поиск пути в сети
    void findPath() {
        if (network.empty()) {
            cout << "Сеть пуста!\n";
            return;
        }
        
        viewAll();
        
        cout << "\nПоиск пути в сети:\n";
        int startId = InputValidator::getIntInput("Введите ID начальной точки: ", 1);
        int endId = InputValidator::getIntInput("Введите ID конечной точки: ", 1);
        
        auto [isStartStation, startIdx] = getObjectInfo(startId);
        auto [isEndStation, endIdx] = getObjectInfo(endId);
        
        if (startIdx == -1) {
            cout << "Начальная точка не найдена!\n";
            return;
        }
        
        if (endIdx == -1) {
            cout << "Конечная точка не найдена!\n";
            return;
        }
        
        // Построение графа
        auto graph = buildGraph();
        
        if (graph.find(startId) == graph.end() || graph.find(endId) == graph.end()) {
            cout << "Одна или обе точки не подключены к сети!\n";
            return;
        }
        
        // BFS для поиска пути
        map<int, int> parent;
        map<int, int> parentPipe;
        queue<int> q;
        set<int> visited;
        
        q.push(startId);
        visited.insert(startId);
        parent[startId] = -1;
        
        while (!q.empty()) {
            int current = q.front();
            q.pop();
            
            if (current == endId) {
                break;
            }
            
            if (graph.find(current) != graph.end()) {
                for (const auto& [neighbor, pipeId] : graph[current].connections) {
                    if (visited.find(neighbor) == visited.end()) {
                        visited.insert(neighbor);
                        parent[neighbor] = current;
                        parentPipe[neighbor] = pipeId;
                        q.push(neighbor);
                    }
                }
            }
        }
        
        // Восстановление пути
        if (parent.find(endId) == parent.end()) {
            cout << "Путь не найден!\n";
            return;
        }
        
        vector<int> path;
        vector<int> pipesPath;
        int current = endId;
        
        while (current != -1) {
            path.push_back(current);
            if (current != startId && parentPipe.find(current) != parentPipe.end()) {
                pipesPath.push_back(parentPipe[current]);
            }
            current = parent[current];
        }
        
        reverse(path.begin(), path.end());
        reverse(pipesPath.begin(), pipesPath.end());
        
        // Вывод пути
        cout << "\nНайденный путь:\n";
        for (size_t i = 0; i < path.size(); ++i) {
            auto [isStation, idx] = getObjectInfo(path[i]);
            string type = isStation ? "КС" : "Труба";
            string name = isStation ? stations[idx].name : pipes[idx].name;
            
            cout << (i + 1) << ". " << type << " ID: " << path[i]
                 << " (" << name << ")";
            
            if (i < path.size() - 1) {
                cout << " ->\n";
            }
        }
        
        if (!pipesPath.empty()) {
            cout << "\n\nИспользуемые трубы на пути:\n";
            double totalLength = 0;
            for (size_t i = 0; i < pipesPath.size(); ++i) {
                int pipeIdx = findPipeIndexById(pipesPath[i]);
                if (pipeIdx != -1) {
                    cout << "Труба ID: " << pipesPath[i]
                         << " (" << pipes[pipeIdx].name
                         << "), Длина: " << pipes[pipeIdx].length << " км\n";
                    totalLength += pipes[pipeIdx].length;
                }
            }
            cout << "Общая длина пути: " << totalLength << " км\n";
        }
        
        logger.log("Поиск пути", "От: " + to_string(startId) + " до: " + to_string(endId) +
                  ", Длина пути: " + to_string(pipesPath.size()) + " труб");
    }

public:
    void addPipe() {
        Pipe newPipe;
        newPipe.id = nextPipeId++;
        newPipe.name = InputValidator::getStringInput("Введите название трубы: ");
        newPipe.length = InputValidator::getDoubleInput("Введите длину трубы (км): ", 0.001);
        newPipe.diameter = InputValidator::getDiameterInput("Введите диаметр трубы");
        newPipe.underRepair = false;
        newPipe.inUse = false;
        newPipe.startId = 0;
        newPipe.endId = 0;
        newPipe.startType = STATION_TO_STATION;
        newPipe.endType = STATION_TO_STATION;
        
        pipes.push_back(newPipe);
        cout << "Труба '" << newPipe.name << "' добавлена с ID: " << newPipe.id << "!\n";
        logger.log("Добавлена труба", "ID: " + to_string(newPipe.id) + ", Название: " + newPipe.name);
    }

    void addStation() {
        CompressorStation newStation;
        newStation.id = nextStationId++;
        newStation.name = InputValidator::getStringInput("Введите название КС: ");
        newStation.totalWorkshops = InputValidator::getIntInput("Введите количество цехов: ", 1);
        newStation.activeWorkshops = InputValidator::getIntInput("Введите работающих цехов: ",
                                                               0, newStation.totalWorkshops);
        newStation.stationClass = InputValidator::getIntInput("Введите класс станции: ", 1);
        
        stations.push_back(newStation);
        cout << "КС '" << newStation.name << "' добавлена с ID: " << newStation.id << "!\n";
        logger.log("Добавлена КС", "ID: " + to_string(newStation.id) + ", Название: " + newStation.name);
    }

    void addMultipleObjects(bool isPipe) {
        int count = InputValidator::getIntInput(isPipe ? "Сколько труб добавить? " : "Сколько КС добавить? ", 1, 100);
        for (int i = 0; i < count; ++i) {
            cout << "\n" << (isPipe ? "Добавление трубы " : "Добавление КС ") << (i + 1) << " из " << count << "\n";
            isPipe ? addPipe() : addStation();
        }
        cout << "Добавлено " << count << (isPipe ? " труб" : " КС") << ". Всего: " << (isPipe ? pipes.size() : stations.size()) << "\n";
    }

    void deleteObjects(bool isPipe) {
        vector<int> indices = isPipe ?
            selectMultipleObjects(getPipeIds(), "труб") :
            selectMultipleObjects(getStationIds(), "КС");
            
        if (indices.empty()) return;
        
        // Проверка использования труб в сети перед удалением
        if (isPipe) {
            vector<int> pipesToRemove;
            for (int index : indices) {
                if (pipes[index].inUse) {
                    cout << "Предупреждение: труба ID " << pipes[index].id
                         << " используется в сети и не будет удалена!\n";
                } else {
                    pipesToRemove.push_back(index);
                }
            }
            indices = pipesToRemove;
        }
        
        sort(indices.rbegin(), indices.rend());
        int count = 0;
        
        for (int index : indices) {
            if (isPipe) {
                cout << "Удалена труба: " << pipes[index].name << " (ID: " << pipes[index].id << ")\n";
                logger.log("Удалена труба", "ID: " + to_string(pipes[index].id) + ", Название: " + pipes[index].name);
                pipes.erase(pipes.begin() + index);
            } else {
                // При удалении станции удаляем все соединения с ней
                int stationId = stations[index].id;
                auto it = remove_if(network.begin(), network.end(),
                                   [stationId](const NetworkConnection& conn) {
                                       return conn.startId == stationId || conn.endId == stationId;
                                   });
                network.erase(it, network.end());
                
                // Освобождаем связанные трубы
                for (auto& pipe : pipes) {
                    if (pipe.startId == stationId || pipe.endId == stationId) {
                        pipe.inUse = false;
                        pipe.startId = 0;
                        pipe.endId = 0;
                    }
                }
                
                cout << "Удалена КС: " << stations[index].name << " (ID: " << stations[index].id << ")\n";
                logger.log("Удалена КС", "ID: " + to_string(stations[index].id) + ", Название: " + stations[index].name);
                stations.erase(stations.begin() + index);
            }
            count++;
        }
        
        cout << "Удалено " << count << (isPipe ? " труб" : " КС") << ". Осталось: " << (isPipe ? pipes.size() : stations.size()) << "\n";
    }

    void editPipe() {
        if (pipes.empty()) {
            cout << "Нет доступных труб!\n";
            return;
        }
        
        viewAll();
        int id = InputValidator::getIntInput("Введите ID трубы для редактирования: ", 1);
        int index = findPipeIndexById(id);
        
        if (index == -1) {
            cout << "Труба с ID " << id << " не найдена!\n";
            return;
        }
        
        cout << "Редактирование трубы ID: " << pipes[index].id << " - " << pipes[index].name << endl;
        cout << "1. Изменить статус ремонта\n2. Редактировать параметры\n";
        int choice = InputValidator::getIntInput("Выберите действие: ", 1, 2);
        
        if (choice == 1) {
            pipes[index].underRepair = !pipes[index].underRepair;
            string status = pipes[index].underRepair ? "В ремонте" : "Работает";
            cout << "Статус ремонта изменен на: " << status << endl;
            
            // Если труба в ремонте и используется в сети
            if (pipes[index].underRepair && pipes[index].inUse) {
                cout << "Внимание: труба используется в сети!\n";
            }
            
            logger.log("Изменен статус трубы", "ID: " + to_string(pipes[index].id) + ", Статус: " + status);
        } else {
            pipes[index].name = InputValidator::getStringInput("Введите новое название трубы: ");
            pipes[index].length = InputValidator::getDoubleInput("Введите новую длину трубы (км): ", 0.001);
            
            // Если труба не используется в сети, можно изменить диаметр
            if (!pipes[index].inUse) {
                pipes[index].diameter = InputValidator::getDiameterInput("Введите новый диаметр трубы");
            } else {
                cout << "Диаметр нельзя изменить, так как труба используется в сети.\n";
            }
            
            cout << "Параметры трубы обновлены!\n";
            logger.log("Обновлена труба", "ID: " + to_string(pipes[index].id) + ", Новое название: " + pipes[index].name);
        }
    }

    void editStation() {
        if (stations.empty()) {
            cout << "Нет доступных КС!\n";
            return;
        }
        
        viewAll();
        int id = InputValidator::getIntInput("Введите ID КС для редактирования: ", 1);
        int index = findStationIndexById(id);
        
        if (index == -1) {
            cout << "КС с ID " << id << " не найдена!\n";
            return;
        }
        
        cout << "Редактирование КС ID: " << stations[index].id << " - " << stations[index].name << endl;
        cout << "1. Запустить/остановить цех\n2. Редактировать параметры\n";
        int choice = InputValidator::getIntInput("Выберите действие: ", 1, 2);
        
        if (choice == 1) {
            cout << "Текущее состояние: " << stations[index].activeWorkshops
                 << "/" << stations[index].totalWorkshops << " цехов работает\n";
            cout << "1. Запустить цех\n2. Остановить цех\n";
            int action = InputValidator::getIntInput("Выберите действие: ", 1, 2);
            
            if (action == 1 && stations[index].activeWorkshops < stations[index].totalWorkshops) {
                stations[index].activeWorkshops++;
                cout << "Цех запущен! Работает цехов: " << stations[index].activeWorkshops << endl;
                logger.log("Запущен цех КС", "ID: " + to_string(stations[index].id) + ", Работает цехов: " + to_string(stations[index].activeWorkshops));
            } else if (action == 2 && stations[index].activeWorkshops > 0) {
                stations[index].activeWorkshops--;
                cout << "Цех остановлен! Работает цехов: " << stations[index].activeWorkshops << endl;
                logger.log("Остановлен цех КС", "ID: " + to_string(stations[index].id) + ", Работает цехов: " + to_string(stations[index].activeWorkshops));
            } else {
                cout << "Невозможно выполнить операцию!\n";
            }
        } else {
            stations[index].name = InputValidator::getStringInput("Введите новое название КС: ");
            int newTotal = InputValidator::getIntInput("Введите новое количество цехов: ", 1);
            
            if (newTotal < stations[index].activeWorkshops) {
                stations[index].activeWorkshops = newTotal;
            }
            stations[index].totalWorkshops = newTotal;
            stations[index].stationClass = InputValidator::getIntInput("Введите новый класс станции: ", 1);
            
            cout << "Параметры КС обновлены!\n";
            logger.log("Обновлена КС", "ID: " + to_string(stations[index].id) + ", Новое название: " + stations[index].name);
        }
    }

    void searchPipes() {
        if (pipes.empty()) {
            cout << "Нет доступных труб для поиска!\n";
            return;
        }
        
        cout << "\nПоиск труб\n";
        cout << "1. По названию\n";
        cout << "2. По признаку 'в ремонте'\n";
        cout << "3. По использованию в сети\n";
        int choice = InputValidator::getIntInput("Выберите тип поиска: ", 1, 3);
        
        vector<int> results;
        string searchDetails;
        
        if (choice == 1) {
            string searchName = InputValidator::getStringInput("Введите название для поиска: ");
            results = findPipesByName(searchName);
            searchDetails = "Поиск по названию: " + searchName;
        } else if (choice == 2) {
            cout << "1. Трубы в ремонте\n";
            cout << "2. Трубы не в ремонте\n";
            int repairChoice = InputValidator::getIntInput("Выберите статус: ", 1, 2);
            bool searchRepairStatus = (repairChoice == 1);
            results = findPipesByRepairStatus(searchRepairStatus);
            searchDetails = "Поиск по статусу ремонта: " + string(searchRepairStatus ? "в ремонте" : "не в ремонте");
        } else {
            cout << "1. Трубы в сети\n";
            cout << "2. Свободные трубы\n";
            int useChoice = InputValidator::getIntInput("Выберите статус: ", 1, 2);
            bool searchUseStatus = (useChoice == 1);
            
            for (size_t i = 0; i < pipes.size(); ++i) {
                if (pipes[i].inUse == searchUseStatus) {
                    results.push_back(i);
                }
            }
            searchDetails = "Поиск по использованию в сети: " + string(searchUseStatus ? "в сети" : "свободные");
        }
        
        displayObjects(results, {});
        logger.log("Поиск труб", searchDetails + ", Найдено: " + to_string(results.size()));
    }

    void searchStations() {
        if (stations.empty()) {
            cout << "Нет доступных КС для поиска!\n";
            return;
        }
        
        cout << "\nПоиск КС\n";
        cout << "1. По названию\n";
        cout << "2. По проценту незадействованных цехов\n";
        int choice = InputValidator::getIntInput("Выберите тип поиска: ", 1, 2);
        
        vector<int> results;
        string searchDetails;
        
        if (choice == 1) {
            string searchName = InputValidator::getStringInput("Введите название для поиска: ");
            results = findStationsByName(searchName);
            searchDetails = "Поиск по названию: " + searchName;
        } else {
            cout << "1. КС с процентом незадействованных цехов БОЛЬШЕ заданного\n";
            cout << "2. КС с процентом незадействованных цехов МЕНЬШЕ заданного\n";
            cout << "3. КС с процентом незадействованных цехов РАВНЫМ заданному\n";
            int percentChoice = InputValidator::getIntInput("Выберите тип сравнения: ", 1, 3);
            double targetPercent = InputValidator::getDoubleInput("Введите процент незадействованных цехов (0-100): ", 0, 100);
            results = findStationsByInactivePercent(targetPercent, percentChoice);
            searchDetails = "Поиск по проценту: " + to_string(targetPercent) + "%, Тип: " + to_string(percentChoice);
        }
        
        displayObjects({}, results);
        logger.log("Поиск КС", searchDetails + ", Найдено: " + to_string(results.size()));
    }

    void viewAll() const {
        vector<int> allPipeIndices, allStationIndices;
        for (int i = 0; i < pipes.size(); ++i) allPipeIndices.push_back(i);
        for (int i = 0; i < stations.size(); ++i) allStationIndices.push_back(i);
        displayObjects(allPipeIndices, allStationIndices);
    }

    void saveData() {
        string filename = InputValidator::getStringInput("Введите имя файла для сохранения: ");
        if (filename.find('.') == string::npos) {
            filename += ".txt";
        }
        
        ofstream file(filename);
        if (!file.is_open()) {
            cout << "Ошибка: невозможно создать файл " << filename << endl;
            return;
        }
        
        file << "NEXT_PIPE_ID " << nextPipeId << endl;
        file << "NEXT_STATION_ID " << nextStationId << endl;
        
        file << "PIPES " << pipes.size() << endl;
        for (const auto& pipe : pipes) {
            file << pipe.id << endl << pipe.name << endl << pipe.length << endl
                 << pipe.diameter << endl << pipe.underRepair << endl
                 << pipe.inUse << endl << pipe.startId << endl << pipe.endId << endl
                 << pipe.startType << endl << pipe.endType << endl;
        }
        
        file << "STATIONS " << stations.size() << endl;
        for (const auto& station : stations) {
            file << station.id << endl << station.name << endl << station.totalWorkshops << endl
                 << station.activeWorkshops << endl << station.stationClass << endl;
        }
        
        file << "NETWORK " << network.size() << endl;
        for (const auto& conn : network) {
            file << conn.pipeId << endl << conn.startId << endl << conn.endId << endl
                 << conn.startType << endl << conn.endType << endl;
        }
        
        file.close();
        cout << "Данные сохранены в файл: " << fs::absolute(filename) << endl;
        logger.log("Сохранение данных", "Файл: " + filename +
                  ", Трубы: " + to_string(pipes.size()) +
                  ", КС: " + to_string(stations.size()) +
                  ", Соединения: " + to_string(network.size()));
    }

    void loadData() {
        string filename = InputValidator::getStringInput("Введите имя файла для загрузки: ");
        
        ifstream file(filename);
        if (!file.is_open()) {
            cout << "Ошибка: файл " << filename << " не найден.\n";
            return;
        }
        
        pipes.clear();
        stations.clear();
        network.clear();
        
        string header;
        size_t count;
        
        file >> header >> nextPipeId;
        if (header != "NEXT_PIPE_ID") {
            file.seekg(0);
            nextPipeId = 1;
            nextStationId = 1;
        } else {
            file >> header >> nextStationId;
        }
        
        file >> header >> count;
        if (header != "PIPES") {
            cout << "Ошибка: неверный формат файла.\n";
            return;
        }
        file.ignore();
        
        for (size_t i = 0; i < count; ++i) {
            Pipe pipe;
            file >> pipe.id;
            file.ignore();
            getline(file, pipe.name);
            file >> pipe.length >> pipe.diameter >> pipe.underRepair
                 >> pipe.inUse >> pipe.startId >> pipe.endId
                 >> pipe.startType >> pipe.endType;
            file.ignore();
            pipes.push_back(pipe);
        }
        
        file >> header >> count;
        if (header != "STATIONS") {
            cout << "Ошибка: неверный формат файла.\n";
            return;
        }
        file.ignore();
        
        for (size_t i = 0; i < count; ++i) {
            CompressorStation station;
            file >> station.id;
            file.ignore();
            getline(file, station.name);
            file >> station.totalWorkshops >> station.activeWorkshops >> station.stationClass;
            file.ignore();
            
            if (station.activeWorkshops > station.totalWorkshops) {
                station.activeWorkshops = station.totalWorkshops;
            }
            
            stations.push_back(station);
        }
        
        // Загрузка сети (если есть)
        if (file >> header >> count) {
            if (header == "NETWORK") {
                file.ignore();
                for (size_t i = 0; i < count; ++i) {
                    NetworkConnection conn;
                    file >> conn.pipeId >> conn.startId >> conn.endId
                         >> conn.startType >> conn.endType;
                    file.ignore();
                    network.push_back(conn);
                }
            }
        }
        
        file.close();
        cout << "Данные загружены из файла: " << fs::absolute(filename) << endl;
        cout << "Загружено труб: " << pipes.size() << ", КС: " << stations.size()
             << ", Соединений: " << network.size() << endl;
        logger.log("Загрузка данных", "Файл: " + filename +
                  ", Трубы: " + to_string(pipes.size()) +
                  ", КС: " + to_string(stations.size()) +
                  ", Соединения: " + to_string(network.size()));
    }

    void run() {
        logger.log("Запуск программы");
        
        while (true) {
            cout << "\nСистема управления трубопроводом\n"
                 << "1. Добавить трубу\n2. Добавить КС\n3. Добавить несколько труб\n4. Добавить несколько КС\n"
                 << "5. Просмотр всех объектов\n6. Редактировать трубу\n7. Редактировать КС\n"
                 << "8. Удалить трубу\n9. Удалить КС\n10. Удалить несколько труб\n11. Удалить несколько КС\n"
                 << "12. Поиск труб\n13. Поиск КС\n14. Сохранить данные\n15. Загрузить данные\n"
                 << "16. Соединить объекты (создать сеть)\n17. Отключить трубу от сети\n"
                 << "18. Просмотр сети\n19. Топологическая сортировка КС\n"
                 << "20. Поиск пути в сети\n0. Выход\n";
            
            int choice = InputValidator::getIntInput("Выберите действие: ", 0, 20);
            logger.log("Выбор меню", "Действие: " + to_string(choice));
            
            switch (choice) {
                case 1: addPipe(); break;
                case 2: addStation(); break;
                case 3: addMultipleObjects(true); break;
                case 4: addMultipleObjects(false); break;
                case 5: viewAll(); break;
                case 6: editPipe(); break;
                case 7: editStation(); break;
                case 8: deleteObjects(true); break;
                case 9: deleteObjects(false); break;
                case 10: deleteObjects(true); break;
                case 11: deleteObjects(false); break;
                case 12: searchPipes(); break;
                case 13: searchStations(); break;
                case 14: saveData(); break;
                case 15: loadData(); break;
                case 16: connectObjects(); break;
                case 17: disconnectPipe(); break;
                case 18: viewNetwork(); break;
                case 19: topologicalSort(); break;
                case 20: findPath(); break;
                case 0:
                    cout << "Выход из программы.\n";
                    logger.log("Выход из программы");
                    return;
            }
        }
    }
};

int main() {
    PipelineSystem system;
    system.run();
    return 0;
}
