#ifndef STOCK_MANAGER_H
#define STOCK_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <string>
#include <ArduinoJson.h>

// Enum to define the type of financial asset
enum AssetType {
    STOCK,
    CRYPTO,
    INDEX
};

// Struct to hold all data for a single financial asset
struct Asset {
    String symbol;
    AssetType type;
    String name;
    float price;
    float change_percent;
    float day_high;
    float day_low;
    unsigned long volume;
    unsigned long last_update;
    bool data_valid;
    String currency; // e.g., "USD"

    // Default constructor
    Asset() : price(0), change_percent(0), day_high(0), day_low(0), volume(0), last_update(0), data_valid(false) {}
};

#include <freertos/semphr.h>

class StockManager {
    friend void fetchStockDataBatchTask(void* p);
public:
    StockManager();

    void begin();
    void loop();

    // Asset Management
    bool addAsset(const String& symbol);
    bool removeAsset(const String& symbol);
    void reorderAssets(const std::vector<String>& symbols);
    void clearAssets();
    const std::vector<Asset>& getAssets() const;

    // Data Fetching
    void fetchData();

    // Display
    String getMarqueeLine();
    void nextPage();
    void previousPage();

    // Configuration
    void setApiKey(const String& key);
    void setRefreshInterval(unsigned long interval); // in minutes
    void setEnabled(bool enabled);

    // Status
    int getApiUsage() const;
    bool isMarketOpen() const;

private:
    std::vector<Asset> _assets;
    String _api_key;
    unsigned long _refresh_interval_ms;
    unsigned long _last_fetch_time;
    bool _enabled;
    bool _is_fetching;

    int _current_asset_index;
    int _current_page_index;

    int _api_usage_count;

    SemaphoreHandle_t _task_mutex;
    volatile int _running_tasks;

    // Private methods
    AssetType getAssetType(const String& symbol);
    void fetchBatchData(const std::vector<String>& symbols, AssetType type);
    void parseJsonResponse(JsonDocument& doc, AssetType type);
    bool isStockMarketOpen();
    bool isCryptoMarketOpen();
};

#endif // STOCK_MANAGER_H
