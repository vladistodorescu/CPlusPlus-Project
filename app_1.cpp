//
// app_1.cpp — Hotel receptionist application (Student: Istodorescu Vlad)
// =============================================================================
//
// Single-file build. No std::cin — everything is driven by argv.
//
// Commands:
//   ./app_1 view_rooms
//   ./app_1 add_room          <room_number> <type> <capacity> <price_per_night>
//   ./app_1 delete_room       <room_number>
//   ./app_1 modify_room       <price|capacity|type> <room_number> <new_value>
//   ./app_1 check_availability <check_in_dd-mm-yyyy> <check_out_dd-mm-yyyy>
//   ./app_1 view_bookings
//
// Data files (in the working directory):
//   camere.txt     — current rooms     (managed here)
//   rezervari.txt  — confirmed bookings (managed by app_2, read here)
//

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// =============================================================================
// Utility helpers
// =============================================================================
namespace utils {

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream iss(s);
    while (std::getline(iss, item, delim)) out.push_back(item);
    return out;
}

// Names / types may contain spaces; we encode spaces as '_' on disk so the
// space-separated file format stays unambiguous, then decode on display.
inline std::string encode(std::string s) {
    std::replace(s.begin(), s.end(), ' ', '_');
    return s;
}
inline std::string decode(std::string s) {
    std::replace(s.begin(), s.end(), '_', ' ');
    return s;
}

} // namespace utils

// =============================================================================
// BaseEntity — abstract base for serializable domain objects (INHERITANCE)
// =============================================================================
class BaseEntity {
public:
    virtual ~BaseEntity() = default;
    virtual std::string serialize() const = 0;
    virtual void deserialize(const std::string& line) = 0;
};

// =============================================================================
// Date — value type with comparison + nights-between
// =============================================================================
class Date {
    int day = 1, month = 1, year = 2000;
public:
    Date() = default;
    Date(int d, int m, int y) : day(d), month(m), year(y) {}

    int getDay()   const { return day; }
    int getMonth() const { return month; }
    int getYear()  const { return year; }

    static Date today() {
        std::time_t t = std::time(nullptr);
        std::tm* now = std::localtime(&t);
        return Date(now->tm_mday, now->tm_mon + 1, now->tm_year + 1900);
    }

    std::string serialize() const {
        std::ostringstream oss;
        oss << day << "-" << month << "-" << year;
        return oss.str();
    }

    // Strict parse — used for argv AND file reads. Throws on invalid input.
    void deserialize(const std::string& line) {
        char d1 = 0, d2 = 0;
        std::istringstream iss(line);
        if (!(iss >> day >> d1 >> month >> d2 >> year) || d1 != '-' || d2 != '-')
            throw std::runtime_error("Invalid date (expected DD-MM-YYYY): '" + line + "'");
        if (month < 1 || month > 12)
            throw std::runtime_error("Invalid month in date '" + line + "'");
        if (day < 1 || day > 31)
            throw std::runtime_error("Invalid day in date '" + line + "'");
        if (year < 1900 || year > 9999)
            throw std::runtime_error("Invalid year in date '" + line + "'");
    }

    // Number of nights between *this and other (other - *this in days).
    int nightsUntil(const Date& other) const {
        std::tm a{}, b{};
        a.tm_mday = day;       a.tm_mon = month - 1;       a.tm_year = year - 1900;
        b.tm_mday = other.day; b.tm_mon = other.month - 1; b.tm_year = other.year - 1900;
        std::time_t ta = std::mktime(&a);
        std::time_t tb = std::mktime(&b);
        return static_cast<int>(std::difftime(tb, ta) / 86400.0);
    }

    // OPERATOR OVERLOADS
    friend std::ostream& operator<<(std::ostream& os, const Date& d) {
        os << std::setw(2) << std::setfill('0') << d.day << "-"
           << std::setw(2) << std::setfill('0') << d.month << "-"
           << d.year;
        return os;
    }
    friend bool operator<(const Date& a, const Date& b) {
        if (a.year  != b.year)  return a.year  < b.year;
        if (a.month != b.month) return a.month < b.month;
        return a.day < b.day;
    }
    friend bool operator==(const Date& a, const Date& b) {
        return a.year == b.year && a.month == b.month && a.day == b.day;
    }
    friend bool operator<=(const Date& a, const Date& b) { return a < b || a == b; }
};

// =============================================================================
// Room : BaseEntity (INHERITANCE)
// =============================================================================
//
// File line:  <number> <encoded_type> <capacity> <price>
//
class Room : public BaseEntity {
    std::string number;
    std::string type;     // "single", "double", "suite", "family", ...
    int capacity = 0;
    double pricePerNight = 0.0;
public:
    Room() = default;
    Room(std::string n, std::string t, int cap, double p)
        : number(std::move(n)), type(std::move(t)), capacity(cap), pricePerNight(p) {}

    const std::string& getNumber()   const { return number; }
    const std::string& getType()     const { return type; }
    int                getCapacity() const { return capacity; }
    double             getPrice()    const { return pricePerNight; }

    void setType(std::string t)    { type = std::move(t); }
    void setCapacity(int c)        { capacity = c; }
    void setPrice(double p)        { pricePerNight = p; }

    std::string serialize() const override {
        std::ostringstream oss;
        oss << number << " " << utils::encode(type) << " " << capacity << " "
            << std::fixed << std::setprecision(2) << pricePerNight;
        return oss.str();
    }
    void deserialize(const std::string& line) override {
        std::istringstream iss(line);
        std::string encType;
        if (!(iss >> number >> encType >> capacity >> pricePerNight))
            throw std::runtime_error("Malformed room line: '" + line + "'");
        type = utils::decode(encType);
    }

    friend std::ostream& operator<<(std::ostream& os, const Room& r) {
        os << "Room " << r.number << "  |  " << r.type
           << "  |  capacity: " << r.capacity
           << "  |  " << std::fixed << std::setprecision(2) << r.pricePerNight << " / night";
        return os;
    }
    bool operator==(const Room& other) const { return number == other.number; }
};

// =============================================================================
// Booking : BaseEntity (INHERITANCE + composition of Date)
// =============================================================================
//
// File line:  <id>|<room_number>|<encoded_guest_name>|<checkIn>|<checkOut>|<total>
//
class Booking : public BaseEntity {
    std::string id;
    std::string roomNumber;
    std::string guestName;
    Date checkIn;
    Date checkOut;
    double totalPrice = 0.0;
public:
    Booking() = default;
    Booking(std::string i, std::string rn, std::string g, Date in, Date out, double t)
        : id(std::move(i)), roomNumber(std::move(rn)), guestName(std::move(g)),
          checkIn(in), checkOut(out), totalPrice(t) {}

    const std::string& getId()         const { return id; }
    const std::string& getRoomNumber() const { return roomNumber; }
    const std::string& getGuestName()  const { return guestName; }
    const Date& getCheckIn()  const { return checkIn; }
    const Date& getCheckOut() const { return checkOut; }
    double getTotalPrice()    const { return totalPrice; }

    // [a, b) overlaps [c, d) iff a < d && c < b
    bool overlaps(const Date& otherIn, const Date& otherOut) const {
        return checkIn < otherOut && otherIn < checkOut;
    }

    std::string serialize() const override {
        std::ostringstream oss;
        oss << id << "|" << roomNumber << "|" << utils::encode(guestName) << "|"
            << checkIn.serialize() << "|" << checkOut.serialize() << "|"
            << std::fixed << std::setprecision(2) << totalPrice;
        return oss.str();
    }
    void deserialize(const std::string& line) override {
        auto parts = utils::split(line, '|');
        if (parts.size() != 6) throw std::runtime_error("Malformed booking line: '" + line + "'");
        id         = parts[0];
        roomNumber = parts[1];
        guestName  = utils::decode(parts[2]);
        checkIn.deserialize(parts[3]);
        checkOut.deserialize(parts[4]);
        totalPrice = std::stod(parts[5]);
    }

    friend std::ostream& operator<<(std::ostream& os, const Booking& b) {
        os << "[" << b.id << "] Room " << b.roomNumber
           << "  |  guest: " << b.guestName
           << "  |  " << b.checkIn << " -> " << b.checkOut
           << "  |  total: " << std::fixed << std::setprecision(2) << b.totalPrice;
        return os;
    }
};

// =============================================================================
// FileHelper<T>  —  TEMPLATE CLASS (used for both Room and Booking)
// =============================================================================
template <typename T>
class FileHelper {
public:
    static std::vector<T> readAll(const std::string& path) {
        std::vector<T> items;
        std::ifstream in(path);
        if (!in.is_open()) return items;

        int n = 0;
        if (!(in >> n)) return items;
        in.ignore();

        for (int i = 0; i < n; ++i) {
            std::string line;
            if (!std::getline(in, line)) break;
            T item;
            item.deserialize(line);
            items.push_back(item);
        }
        return items;
    }
    static void writeAll(const std::string& path, const std::vector<T>& items) {
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open())
            throw std::runtime_error("Could not open file for writing: " + path);
        out << items.size() << "\n";
        for (const auto& item : items) out << item.serialize() << "\n";
    }
};

// =============================================================================
// RoomManager — wraps camere.txt (CRUD on rooms)
// =============================================================================
class RoomManager {
    std::string path;
    std::vector<Room> rooms;

    void save() { FileHelper<Room>::writeAll(path, rooms); }

    std::vector<Room>::iterator findOrThrow(const std::string& number) {
        auto it = std::find_if(rooms.begin(), rooms.end(),
            [&](const Room& r) { return r.getNumber() == number; });
        if (it == rooms.end())
            throw std::runtime_error("No room with number '" + number + "'.");
        return it;
    }
public:
    explicit RoomManager(std::string p) : path(std::move(p)) {
        rooms = FileHelper<Room>::readAll(path);
    }

    const std::vector<Room>& getRooms() const { return rooms; }

    void viewRooms() const {
        if (rooms.empty()) { std::cout << "No rooms registered.\n"; return; }
        std::cout << "Rooms (" << rooms.size() << "):\n";
        for (const auto& r : rooms) std::cout << "  " << r << "\n";
    }

    void addRoom(const Room& r) {
        auto it = std::find_if(rooms.begin(), rooms.end(),
            [&](const Room& q) { return q.getNumber() == r.getNumber(); });
        if (it != rooms.end())
            throw std::runtime_error("Room '" + r.getNumber() + "' already exists.");
        rooms.push_back(r);
        save();
    }

    void deleteRoom(const std::string& number) {
        auto it = findOrThrow(number);
        rooms.erase(it);
        save();
    }

    void modifyType(const std::string& number, const std::string& newType) {
        if (newType.empty()) throw std::runtime_error("Type must not be empty.");
        findOrThrow(number)->setType(newType);
        save();
    }
    void modifyCapacity(const std::string& number, int newCap) {
        if (newCap < 1) throw std::runtime_error("Capacity must be >= 1.");
        findOrThrow(number)->setCapacity(newCap);
        save();
    }
    void modifyPrice(const std::string& number, double newPrice) {
        if (newPrice < 0) throw std::runtime_error("Price must be non-negative.");
        findOrThrow(number)->setPrice(newPrice);
        save();
    }
};

// =============================================================================
// BookingManager — wraps rezervari.txt
// =============================================================================
class BookingManager {
    std::string path;
    std::vector<Booking> bookings;

    void save() { FileHelper<Booking>::writeAll(path, bookings); }
public:
    explicit BookingManager(std::string p) : path(std::move(p)) {
        bookings = FileHelper<Booking>::readAll(path);
    }

    const std::vector<Booking>& getBookings() const { return bookings; }

    bool isRoomAvailable(const std::string& roomNumber, const Date& in, const Date& out) const {
        for (const auto& b : bookings) {
            if (b.getRoomNumber() == roomNumber && b.overlaps(in, out))
                return false;
        }
        return true;
    }

    void addBooking(const Booking& b) {
        bookings.push_back(b);
        save();
    }

    void cancelBooking(const std::string& id) {
        auto it = std::find_if(bookings.begin(), bookings.end(),
            [&](const Booking& b) { return b.getId() == id; });
        if (it == bookings.end())
            throw std::runtime_error("No booking with id '" + id + "'.");
        bookings.erase(it);
        save();
    }

    // Next ID = "B" + (max existing numeric suffix + 1), zero-padded to 4 digits.
    std::string nextBookingId() const {
        int maxN = 0;
        for (const auto& b : bookings) {
            const std::string& id = b.getId();
            if (id.size() > 1 && id[0] == 'B') {
                try {
                    int n = std::stoi(id.substr(1));
                    if (n > maxN) maxN = n;
                } catch (...) { /* ignore non-numeric */ }
            }
        }
        std::ostringstream oss;
        oss << "B" << std::setw(4) << std::setfill('0') << (maxN + 1);
        return oss.str();
    }
};

// =============================================================================
// main — receptionist commands
// =============================================================================
namespace {
constexpr const char* kRoomsFile    = "camere.txt";
constexpr const char* kBookingsFile = "rezervari.txt";

void printUsage() {
    std::cout <<
      "Usage:\n"
      "  app_1 view_rooms\n"
      "  app_1 add_room          <room_number> <type> <capacity> <price_per_night>\n"
      "  app_1 delete_room       <room_number>\n"
      "  app_1 modify_room       <price|capacity|type> <room_number> <new_value>\n"
      "  app_1 check_availability <check_in_dd-mm-yyyy> <check_out_dd-mm-yyyy>\n"
      "  app_1 view_bookings\n"
      "\n"
      "Notes: <type> cannot contain spaces (use '_' — displays as space).\n"
      "       Dates are DD-MM-YYYY, e.g. 15-08-2026.\n";
}

int parseInt(const std::string& arg, const std::string& label) {
    try {
        std::size_t pos = 0;
        int v = std::stoi(arg, &pos);
        if (pos != arg.size()) throw std::invalid_argument("trailing chars");
        return v;
    } catch (...) {
        throw std::runtime_error("Invalid integer for " + label + ": '" + arg + "'");
    }
}
double parseDouble(const std::string& arg, const std::string& label) {
    try {
        std::size_t pos = 0;
        double v = std::stod(arg, &pos);
        if (pos != arg.size()) throw std::invalid_argument("trailing chars");
        return v;
    } catch (...) {
        throw std::runtime_error("Invalid number for " + label + ": '" + arg + "'");
    }
}
Date parseDate(const std::string& arg) {
    Date d;
    d.deserialize(arg);
    return d;
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) { printUsage(); return 1; }

    // SMART POINTERS (bonus)
    auto rooms    = std::make_unique<RoomManager>(kRoomsFile);
    auto bookings = std::make_unique<BookingManager>(kBookingsFile);

    const std::string cmd = argv[1];

    try {
        if (cmd == "view_rooms") {
            if (argc != 2) throw std::runtime_error("view_rooms takes no arguments.");
            rooms->viewRooms();
        }
        else if (cmd == "add_room") {
            if (argc != 6)
                throw std::runtime_error("add_room expects: <room_number> <type> <capacity> <price_per_night>");
            std::string number = argv[2], type = argv[3];
            int    cap   = parseInt(argv[4], "capacity");
            double price = parseDouble(argv[5], "price_per_night");
            if (number.empty()) throw std::runtime_error("Room number must not be empty.");
            if (type.empty())   throw std::runtime_error("Type must not be empty.");
            if (cap < 1)        throw std::runtime_error("Capacity must be >= 1.");
            if (price < 0)      throw std::runtime_error("Price must be non-negative.");
            rooms->addRoom(Room(number, type, cap, price));
            std::cout << "Added room '" << number << "'.\n";
        }
        else if (cmd == "delete_room") {
            if (argc != 3) throw std::runtime_error("delete_room expects: <room_number>");
            // Also reject deletion if there are bookings tied to this room.
            for (const auto& b : bookings->getBookings()) {
                if (b.getRoomNumber() == argv[2])
                    throw std::runtime_error(
                        "Room '" + std::string(argv[2]) +
                        "' has active bookings (e.g. " + b.getId() + ") — cancel them first.");
            }
            rooms->deleteRoom(argv[2]);
            std::cout << "Deleted room '" << argv[2] << "'.\n";
        }
        else if (cmd == "modify_room") {
            if (argc != 5)
                throw std::runtime_error("modify_room expects: <price|capacity|type> <room_number> <new_value>");
            std::string field = argv[2], number = argv[3];
            if      (field == "price")    rooms->modifyPrice(number, parseDouble(argv[4], "price"));
            else if (field == "capacity") rooms->modifyCapacity(number, parseInt(argv[4], "capacity"));
            else if (field == "type")     rooms->modifyType(number, argv[4]);
            else throw std::runtime_error("Field must be 'price', 'capacity' or 'type'.");
            std::cout << "Modified " << field << " of room '" << number << "'.\n";
        }
        else if (cmd == "check_availability") {
            if (argc != 4)
                throw std::runtime_error("check_availability expects: <check_in> <check_out>");
            Date in  = parseDate(argv[2]);
            Date out = parseDate(argv[3]);
            if (!(in < out))
                throw std::runtime_error("check_in must be strictly before check_out.");
            std::cout << "Available rooms for " << in << " -> " << out << ":\n";
            int count = 0;
            for (const auto& r : rooms->getRooms()) {
                if (bookings->isRoomAvailable(r.getNumber(), in, out)) {
                    std::cout << "  " << r << "\n";
                    ++count;
                }
            }
            std::cout << count << " available room(s).\n";
        }
        else if (cmd == "view_bookings") {
            if (argc != 2) throw std::runtime_error("view_bookings takes no arguments.");
            const auto& bs = bookings->getBookings();
            if (bs.empty()) { std::cout << "No bookings yet.\n"; return 0; }
            std::cout << "Bookings (" << bs.size() << "):\n";
            for (const auto& b : bs) std::cout << "  " << b << "\n";
        }
        else {
            std::cerr << "Unknown command: '" << cmd << "'\n\n";
            printUsage();
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
