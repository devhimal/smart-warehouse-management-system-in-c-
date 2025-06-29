#include <iostream>
#include <memory>
#include <vector>
#include <queue>
#include <map>
#include <set>
#include <limits>
#include <fstream>
#include <cppconn/driver.h>
#include <cppconn/connection.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>

using namespace std;

const int LOW_STOCK_THRESHOLD = 10;

struct Order
{
    int priority;
    int item_id;
    int quantity;
    bool operator<(const Order &other) const
    {
        return priority < other.priority; // Max-Heap based on priority
    }
};

class WarehouseSystem
{
private:
    priority_queue<Order> orderQueue;
    map<int, int> stockMap;
    map<int, vector<pair<int, int>>> warehouseGraph;
    set<int> lowStockItems;

    shared_ptr<sql::Connection> connectDB()
    {
        sql::Driver *driver = get_driver_instance();
        shared_ptr<sql::Connection> con(driver->connect("tcp://127.0.0.1:3306", "admin_user", "StrongPass123"));
        con->setSchema("warehouse_db");
        return con;
    }

    void updateStockMap()
    {
        stockMap.clear();
        auto con = connectDB();
        auto stmt = con->prepareStatement("SELECT item_id, stock_quantity FROM items");
        auto res = stmt->executeQuery();
        while (res->next())
        {
            stockMap[res->getInt("item_id")] = res->getInt("stock_quantity");
        }
    }

    void checkLowStock()
    {
        updateStockMap();
        lowStockItems.clear();
        for (auto &[id, qty] : stockMap)
        {
            if (qty < LOW_STOCK_THRESHOLD)
                lowStockItems.insert(id);
        }
    }

    void dijkstra(int src)
    {
        map<int, int> dist;
        for (auto &[node, _] : warehouseGraph)
        {
            dist[node] = numeric_limits<int>::max();
        }
        dist[src] = 0;
        set<pair<int, int>> pq; // (distance, node)
        pq.insert({0, src});

        while (!pq.empty())
        {
            auto [d, u] = *pq.begin();
            pq.erase(pq.begin());
            for (auto &[v, w] : warehouseGraph[u])
            {
                if (dist[u] + w < dist[v])
                {
                    pq.erase({dist[v], v});
                    dist[v] = dist[u] + w;
                    pq.insert({dist[v], v});
                }
            }
        }

        cout << "📍 Shortest distances from shelf " << src << ":\n";
        for (auto &[node, d] : dist)
        {
            cout << "To shelf " << node << ": " << (d == numeric_limits<int>::max() ? -1 : d) << " steps\n";
        }
    }

public:
    void showMenu()
    {
        cout << "\n--- Smart Warehouse Management System ---\n";
        cout << "1. Add Item\n";
        cout << "2. View Items\n";
        cout << "3. Record Incoming Stock\n";
        cout << "4. Record Outgoing Stock\n";
        cout << "5. View Transaction History\n";
        cout << "6. Add Order to Queue\n";
        cout << "7. Process Order Queue\n";
        cout << "8. Check Low Stock Alerts\n";
        cout << "9. Export CSV Report\n";
        cout << "10. Shortest Path Between Shelves\n";
        cout << "0. Exit\n";
        cout << "Select option: ";
    }

    void addItem()
    {
        string name, category, vendor;
        int quantity;
        cout << "Item name: ";
        cin >> ws;
        getline(cin, name);
        cout << "Category: ";
        getline(cin, category);
        cout << "Initial quantity: ";
        cin >> quantity;
        cout << "Vendor name: ";
        cin >> ws;
        getline(cin, vendor);

        auto con = connectDB();
        auto stmt = con->prepareStatement("INSERT INTO items (item_name, category, stock_quantity, vendor_name) VALUES (?, ?, ?, ?)");
        stmt->setString(1, name);
        stmt->setString(2, category);
        stmt->setInt(3, quantity);
        stmt->setString(4, vendor);
        stmt->executeUpdate();
        cout << "✅ Item added.\n";
    }

    void exportCSVReport()
    {
        updateStockMap();
        ofstream file("inventory_report.csv");
        file << "Item ID,Stock Quantity\n";
        for (auto &[id, qty] : stockMap)
        {
            file << id << "," << qty << "\n";
        }
        file.close();
        cout << "✅ Report exported to inventory_report.csv\n";
    }

    void viewItems()
    {
        auto con = connectDB();
        auto stmt = con->prepareStatement("SELECT * FROM items");
        auto res = stmt->executeQuery();

        cout << "\n📦 Available Items:\n";
        cout << "ID | Name | Category | Stock | Vendor\n";
        cout << "---------------------------------------------\n";
        while (res->next())
        {
            cout << res->getInt("item_id") << " | "
                 << res->getString("item_name") << " | "
                 << res->getString("category") << " | "
                 << res->getInt("stock_quantity") << " | "
                 << res->getString("vendor_name") << "\n";
        }
    }

    void recordTransaction(const string &type)
    {
        int item_id, quantity;
        string partyName;

        cout << "Item ID: ";
        cin >> item_id;
        cout << "Quantity: ";
        cin >> quantity;

        if (type == "INCOME")
        {
            cout << "Vendor name: ";
            cin >> ws;
            getline(cin, partyName);
        }
        else
        {
            cout << "Client name: ";
            cin >> ws;
            getline(cin, partyName);
        }

        auto con = connectDB();
        auto checkStmt = con->prepareStatement("SELECT stock_quantity FROM items WHERE item_id = ?");
        checkStmt->setInt(1, item_id);
        auto res = checkStmt->executeQuery();

        if (!res->next())
        {
            cout << "❌ Item not found.\n";
            return;
        }

        int stock = res->getInt("stock_quantity");
        int new_stock = (type == "INCOME") ? stock + quantity : stock - quantity;
        if (new_stock < 0)
        {
            cout << "❌ Not enough stock.\n";
            return;
        }

        auto updateStmt = con->prepareStatement("UPDATE items SET stock_quantity = ? WHERE item_id = ?");
        updateStmt->setInt(1, new_stock);
        updateStmt->setInt(2, item_id);
        updateStmt->executeUpdate();

        auto transStmt = con->prepareStatement(
            "INSERT INTO transactions (item_id, quantity, type, party_name) VALUES (?, ?, ?, ?)");
        transStmt->setInt(1, item_id);
        transStmt->setInt(2, (type == "INCOME") ? quantity : -quantity);
        transStmt->setString(3, type);
        transStmt->setString(4, partyName);
        transStmt->executeUpdate();

        cout << "✅ Transaction recorded.\n";
    }

    void viewTransactionHistory()
    {
        auto con = connectDB();
        auto stmt = con->prepareStatement(
            "SELECT t.transaction_id, i.item_name, t.quantity, t.type, t.party_name, t.timestamp FROM transactions t "
            "JOIN items i ON t.item_id = i.item_id ORDER BY t.timestamp DESC");
        auto res = stmt->executeQuery();

        cout << "\n🧾 Transaction History:\n";
        while (res->next())
        {
            cout << "[" << res->getString("timestamp") << "] "
                 << res->getString("type") << " - "
                 << res->getString("item_name") << " | Qty: "
                 << res->getInt("quantity") << " | "
                 << ((res->getString("type") == "INCOME") ? "Vendor: " : "Client: ")
                 << res->getString("party_name") << "\n";
        }
    }

    void addOrderToQueue()
    {
        Order ord;
        cout << "Item ID: ";
        cin >> ord.item_id;
        cout << "Quantity: ";
        cin >> ord.quantity;
        cout << "Priority (higher = urgent): ";
        cin >> ord.priority;
        orderQueue.push(ord);
        cout << "✅ Order added to queue.\n";
    }

    void processOrderQueue()
    {
        if (orderQueue.empty())
        {
            cout << "No pending orders.\n";
            return;
        }

        auto ord = orderQueue.top();
        orderQueue.pop();
        cout << "Processing Order - Item: " << ord.item_id << " Qty: " << ord.quantity << " Priority: " << ord.priority << "\n";

        auto con = connectDB();
        auto checkStmt = con->prepareStatement("SELECT stock_quantity FROM items WHERE item_id = ?");
        checkStmt->setInt(1, ord.item_id);
        auto res = checkStmt->executeQuery();

        if (!res->next() || res->getInt("stock_quantity") < ord.quantity)
        {
            cout << "❌ Cannot fulfill order (out of stock or not found).\n";
            return;
        }

        int new_stock = res->getInt("stock_quantity") - ord.quantity;

        auto updateStmt = con->prepareStatement("UPDATE items SET stock_quantity = ? WHERE item_id = ?");
        updateStmt->setInt(1, new_stock);
        updateStmt->setInt(2, ord.item_id);
        updateStmt->executeUpdate();

        // Ask client name
        string clientName;
        cout << "Enter client name for this order: ";
        cin >> ws;
        getline(cin, clientName);

        auto transStmt = con->prepareStatement("INSERT INTO transactions (item_id, quantity, type, party_name) VALUES (?, ?, 'OUTGOING', ?)");
        transStmt->setInt(1, ord.item_id);
        transStmt->setInt(2, -ord.quantity);
        transStmt->setString(3, clientName);
        transStmt->executeUpdate();

        cout << "✅ Order processed.\n";
    }

    void showLowStockAlerts()
    {
        checkLowStock();
        if (lowStockItems.empty())
        {
            cout << "✅ All items are sufficiently stocked.\n";
        }
        else
        {
            cout << "⚠️ Low Stock Alerts for Item IDs: ";
            for (int id : lowStockItems)
                cout << id << " ";
            cout << "\n";
        }
    }

    void shortestPath()
    {
        int n, edges, u, v, w;
        warehouseGraph.clear();
        cout << "Enter number of shelves (nodes): ";
        cin >> n;
        cout << "Enter number of connections: ";
        cin >> edges;
        cout << "Enter edges as: from to distance\n";
        for (int i = 0; i < edges; ++i)
        {
            cin >> u >> v >> w;
            warehouseGraph[u].push_back({v, w});
            warehouseGraph[v].push_back({u, w});
        }
        int start;
        cout << "Enter starting shelf: ";
        cin >> start;
        dijkstra(start);
    }
};

int main()
{
    WarehouseSystem wms;
    while (true)
    {
        wms.showMenu();
        int choice;
        cin >> choice;
        switch (choice)
        {
        case 1:
            wms.addItem();
            break;
        case 2:
            wms.viewItems();
            break;
        case 3:
            wms.recordTransaction("INCOME");
            break;
        case 4:
            wms.recordTransaction("OUTGOING");
            break;
        case 5:
            wms.viewTransactionHistory();
            break;
        case 6:
            wms.addOrderToQueue();
            break;
        case 7:
            wms.processOrderQueue();
            break;
        case 8:
            wms.showLowStockAlerts();
            break;
        case 9:
            wms.exportCSVReport();
            break;
        case 10:
            wms.shortestPath();
            break;
        case 0:
            cout << "👋 Exiting system.\n";
            return 0;
        default:
            cout << "❗ Invalid choice.\n";
        }
    }
}
