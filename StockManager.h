/**
 * @file StockManager.h
 * @brief Defines the StockManager class for fetching, managing, and displaying financial asset data.
 * @details This file contains the complete definition of the StockManager, which encapsulates all
 * functionality related to the stock ticker feature. This includes managing a list of assets,
 * fetching data from the Financial Modeling Prep API, handling API rate limits, persisting asset
 * lists, and providing data for display. It is designed to be a self-contained module.
 */
#ifndef STOCK_MANAGER_H
#define STOCK_MANAGER_H

#include <Arduino.h>
#include <vector>
#include <string>
#include <map>
#include <ArduinoJson.h>

/** @brief Represents the result of an attempt to add a new financial asset. */
enum AssetAddResult {
    SUCCESS,            /**< The asset was added successfully. */
    ALREADY_EXISTS,     /**< The asset with this symbol is already in the list. */
    INVALID_SYMBOL,     /**< The symbol was not found or is invalid according to the API. */
    ADD_ERROR           /**< A generic error occurred during the process. */
};

/** @brief Holds all relevant data for a single financial asset (e.g., a stock). */
struct Asset {
    String symbol;              /**< The stock ticker symbol (e.g., "AAPL"). */
    String name;                /**< The full name of the company. */
    float price;                /**< The current price. */
    float change_percent;       /**< The percentage change for the day. */
    float day_high;             /**< The highest price for the day. */
    float day_low;              /**< The lowest price for the day. */
    unsigned long volume;       /**< The trading volume for the day. */
    unsigned long last_update;  /**< The timestamp of the last successful data update. */
    bool data_valid;            /**< Flag indicating if the current data is valid and up-to-date. */
    String currency;            /**< The currency the asset is traded in (e.g., "USD"). */
    String exchange;            /**< The exchange the asset is traded on (e.g., "NASDAQ"). */
    String error_reason;        /**< Stores a specific error message if data fetching failed for this asset. */

    /** @brief Default constructor to initialize the asset with safe default values. */
    Asset() : price(0), change_percent(0), day_high(0), day_low(0), volume(0), last_update(0), data_valid(false) {}
};

#include <freertos/semphr.h>

/** @brief Represents the status of a data fetching attempt from the API. */
enum FetchStatus {
    FETCH_SUCCESS,              /**< The data was fetched and parsed successfully. */
    FETCH_CONNECTION_FAILED,    /**< Could not connect to the API server. */
    FETCH_RATE_LIMITED,         /**< The API request was denied due to rate limiting. */
    FETCH_FAILED                /**< A generic failure occurred (e.g., invalid JSON, API error). */
};

// Forward declaration of the class to be used in StockFetchParams
class StockManager;

/** @brief Parameters for the FreeRTOS task that fetches stock data. */
struct StockFetchParams {
    std::vector<String> symbols; /**< A vector of symbols to fetch data for. */
    StockManager* manager;       /**< A pointer to the StockManager instance to operate on. */
};

/**
 * @class StockManager
 * @brief Manages the entire lifecycle of fetching, storing, and displaying stock data.
 * @details This class is the core of the stock ticker feature. It maintains a list of assets,
 * handles periodic fetching of data in a background task, respects API limits, saves the
 * asset list to non-volatile storage, and provides an interface for the display manager
 * to get formatted text for scrolling.
 */
class StockManager {
    friend void fetchStockDataBatchTask(void* p);
    friend void fetchSingleStockTask(void* p);
public:
    /** @brief Constructor for the StockManager. */
    StockManager();

    /** @brief Initializes the manager, loads assets from storage, and sets up timers. */
    void begin();
    /** @brief Main loop function, called repeatedly to check if a data fetch is due. */
    void loop();

    /**
     * @name Asset Management
     * @{
     */
    AssetAddResult addAsset(const String& symbol);
    bool removeAsset(const String& symbol);
    void clearAssets();
    const std::vector<Asset>& getAssets() const;
    void updateAssetsFromJson(const String& jsonString);
    void updateAndSaveAssets(const std::vector<String>& symbols);
    void saveAssets();
    void loadAssets();
    String validateSymbol(const String& symbol);
    /** @} */

    /**
     * @name Data Fetching
     * @{
     */
    /** @brief Initiates an asynchronous fetch of data for all managed assets. */
    void fetchData();
    /** @} */

    /**
     * @name Display
     * @{
     */
    void getMarqueeLine(char* buffer, size_t bufferSize);
    void nextPage();
    void previousPage();
    void resetTicker();
    Asset getCurrentStockInfo() const;
    /** @} */

    /**
     * @name Configuration
     * @{
     */
    void setApiKey(const String& key);
    String getApiKey() const;
    void setRefreshInterval(unsigned long interval); // in minutes
    void setEnabled(bool enabled);
    /** @} */

    /**
     * @name Status
     * @{
     */
    int getApiUsage() const;
    bool isMarketOpen() const;
    bool isTimeSynchronized() const;
    bool isFetching() const;
    bool hasDataBeenUpdated();
    void clearDataUpdatedFlag();
    bool hasAnyValidData() const;
    /** @} */

private:
    // --- Private Members ---
    std::vector<Asset> _assets;         /**< The vector holding all the managed asset data. */
    String _api_key;                    /**< The API key for Financial Modeling Prep. */
    unsigned long _refresh_interval_ms; /**< The data refresh interval in milliseconds. */
    unsigned long _last_fetch_time;     /**< The `millis()` timestamp of the last fetch attempt. */
    bool _enabled;                      /**< Master enable/disable flag for the stock feature. */
    bool _is_fetching;                  /**< Flag indicating if a fetch operation is currently in progress. */

    int _current_asset_index;           /**< Index of the asset currently being shown in the ticker. */
    int _current_page_index;            /**< Page index for the multi-page display of a single asset. */

    int _api_usage_count;               /**< Counter for API calls made today to respect daily limits. */
    int _last_reset_day;                /**< The day of the month when the API usage was last reset. */

    SemaphoreHandle_t _task_mutex;      /**< Mutex to ensure only one fetch task runs at a time. */
    SemaphoreHandle_t _assets_mutex;    /**< Mutex to protect the `_assets` vector from concurrent access. */
    volatile int _running_tasks;        /**< Counter for the number of currently running fetch tasks. */
    bool _data_updated;                 /**< Flag set to true when new data has been successfully fetched. */
    bool _initial_fetch_done;           /**< Flag to ensure the first fetch happens immediately on boot. */

    // --- Private Methods ---
    String fetchExchangeForSymbol(const String& symbol) const;
    FetchStatus fetchDataForSingleSymbol(const std::vector<String>& symbol);
    void fetchDataForMultipleSymbols(const std::vector<String>& symbols);
    void parseJsonResponse(JsonDocument& doc, const std::vector<String>& requested_symbols);
    void resetApiUsageIfNecessary();
    void loadApiUsage();
    void saveApiUsage();
};

#endif // STOCK_MANAGER_H
