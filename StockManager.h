#ifndef STOCK_MANAGER_H
#define STOCK_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <string>
#include <map>
#include <ArduinoJson.h>

// Struct to hold all data for a single financial asset
struct Asset {
    String symbol;
    String name;
    float price;
    float change_percent;
    float day_high;
    float day_low;
    unsigned long volume;
    unsigned long last_update;
    bool data_valid;
    String currency; // e.g., "USD"
    String exchange; // e.g., "NASDAQ"
    String error_reason; // To store specific error messages

    // Default constructor
    Asset() : price(0), change_percent(0), day_high(0), day_low(0), volume(0), last_update(0), data_valid(false) {}
};

#include <freertos/semphr.h>

enum FetchStatus {
    FETCH_SUCCESS,
    FETCH_CONNECTION_FAILED,
    FETCH_RATE_LIMITED,
    FETCH_FAILED
};

// Forward declaration of the class
class StockManager;

// Struct to pass parameters to the FreeRTOS task
struct StockFetchParams {
    std::vector<String> symbols;
    StockManager* manager;
};

class StockManager {
    friend void fetchStockDataBatchTask(void* p);
    friend void fetchSingleStockTask(void* p);
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
    void updateAssetsFromJson(const String& jsonString);
    void updateAndSaveAssets(const std::vector<String>& symbols);
    void saveAssets();
    void loadAssets();

    // Data Fetching
    void fetchData();

    // Display
    String getMarqueeLine();
    void nextPage();
    void previousPage();
    void resetTicker();
    Asset getCurrentStockInfo() const;

    // Configuration
    void setApiKey(const String& key);
    String getApiKey() const;
    void setRefreshInterval(unsigned long interval); // in minutes
    void setEnabled(bool enabled);

    // Status
    int getApiUsage() const;
    bool isMarketOpen() const;
    bool isTimeSynchronized() const;
    bool isFetching() const;
    bool hasDataBeenUpdated();
    void clearDataUpdatedFlag();

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
    SemaphoreHandle_t _assets_mutex;
    volatile int _running_tasks;
    bool _data_updated;

    // Private methods
    String fetchExchangeForSymbol(const String& symbol) const;
    FetchStatus fetchDataForSingleSymbol(const std::vector<String>& symbol);
    void fetchDataForMultipleSymbols(const std::vector<String>& symbols);
    void parseJsonResponse(JsonDocument& doc, const std::vector<String>& requested_symbols);
};

#endif // STOCK_MANAGER_H
