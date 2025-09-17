#include "StockManager.h"
#include "DebugLog.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "timezone.h"

extern bool timeSynchronized;

StockManager::StockManager() :
    _api_key(""),
    _refresh_interval_ms(2 * 60 * 1000), // Default 2 minutes
    _last_fetch_time(0),
    _enabled(false),
    _is_fetching(false),
    _current_asset_index(0),
    _current_page_index(0),
    _api_usage_count(0),
    _running_tasks(0) {
    _task_mutex = xSemaphoreCreateMutex();
}

void StockManager::begin() {
    Log_printf(LOG_LEVEL_INFO, "StockManager initialized.");
}

void StockManager::loop() {
    if (!_enabled || _is_fetching || _api_key.isEmpty()) {
        return;
    }

    unsigned long now = millis();
    if (now - _last_fetch_time > _refresh_interval_ms) {
        fetchData();
    }
}

bool StockManager::addAsset(const String& symbol) {
    for (const auto& asset : _assets) {
        if (asset.symbol.equalsIgnoreCase(symbol)) {
            Log_printf(LOG_LEVEL_WARN, "Asset %s already exists.", symbol.c_str());
            return false;
        }
    }

    Asset newAsset;
    newAsset.symbol = symbol;
    newAsset.type = getAssetType(symbol);
    _assets.push_back(newAsset);
    Log_printf(LOG_LEVEL_INFO, "Added asset: %s", symbol.c_str());
    return true;
}

bool StockManager::removeAsset(const String& symbol) {
    auto it = std::remove_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
        return asset.symbol.equalsIgnoreCase(symbol);
    });

    if (it != _assets.end()) {
        _assets.erase(it, _assets.end());
        Log_printf(LOG_LEVEL_INFO, "Removed asset: %s", symbol.c_str());
        if (_current_asset_index >= _assets.size() && !_assets.empty()) {
            _current_asset_index = _assets.size() - 1;
        }
        return true;
    }
    return false;
}

void StockManager::reorderAssets(const std::vector<String>& symbols) {
    std::vector<Asset> reordered_assets;
    for (const auto& symbol : symbols) {
        for (const auto& asset : _assets) {
            if (asset.symbol.equalsIgnoreCase(symbol)) {
                reordered_assets.push_back(asset);
                break;
            }
        }
    }
    _assets = reordered_assets;
    Log_printf(LOG_LEVEL_INFO, "Assets reordered.");
}

void StockManager::clearAssets() {
    _assets.clear();
    Log_printf(LOG_LEVEL_INFO, "All assets cleared.");
}

const std::vector<Asset>& StockManager::getAssets() const {
    return _assets;
}

// Forward declaration for the FreeRTOS task
void fetchStockDataBatchTask(void* p);

struct StockFetchParams {
    std::vector<String> symbols;
    AssetType type;
    StockManager* manager;
};

void StockManager::fetchData() {
    if (!isMarketOpen()) {
        Log_printf(LOG_LEVEL_INFO, "Market is closed. Skipping stock data fetch.");
        _last_fetch_time = millis();
        return;
    }

    Log_printf(LOG_LEVEL_INFO, "Fetching stock data...");
    _is_fetching = true;
    _last_fetch_time = millis();

    std::vector<String> stocks;
    std::vector<String> cryptos;
    std::vector<String> indices;

    for (const auto& asset : _assets) {
        switch (asset.type) {
            case STOCK:
                stocks.push_back(asset.symbol);
                break;
            case CRYPTO:
                cryptos.push_back(asset.symbol);
                break;
            case INDEX:
                indices.push_back(asset.symbol);
                break;
        }
    }

    xSemaphoreTake(_task_mutex, portMAX_DELAY);
    _running_tasks = 0;
    if (!stocks.empty() && isStockMarketOpen()) {
        StockFetchParams* params = new StockFetchParams{stocks, STOCK, this};
        if (xTaskCreate(fetchStockDataBatchTask, "stockFetch", 8192, params, 1, NULL) == pdPASS) {
            _running_tasks += 1;
        } else {
            delete params;
        }
    }
    if (!cryptos.empty()) {
        StockFetchParams* params = new StockFetchParams{cryptos, CRYPTO, this};
        if (xTaskCreate(fetchStockDataBatchTask, "cryptoFetch", 8192, params, 1, NULL) == pdPASS) {
            _running_tasks += 1;
        } else {
            delete params;
        }
    }
    if (!indices.empty() && isStockMarketOpen()) {
        StockFetchParams* params = new StockFetchParams{indices, INDEX, this};
        if (xTaskCreate(fetchStockDataBatchTask, "indexFetch", 8192, params, 1, NULL) == pdPASS) {
            _running_tasks += 1;
        } else {
            delete params;
        }
    }
    xSemaphoreGive(_task_mutex);

    if (_running_tasks == 0) {
        _is_fetching = false;
    }
}

void StockManager::fetchBatchData(const std::vector<String>& symbols, AssetType type) {
    String symbols_str = "";
    for (const auto& s : symbols) {
        symbols_str += s + ",";
    }
    symbols_str.remove(symbols_str.length() - 1);

    String url;
    switch (type) {
        case STOCK:
            url = "https://financialmodelingprep.com/api/v3/quote/" + symbols_str + "?apikey=" + _api_key;
            break;
        case CRYPTO:
             url = "https://financialmodelingprep.com/api/v3/quote/" + symbols_str + "?apikey=" + _api_key;
            break;
        case INDEX:
            url = "https://financialmodelingprep.com/api/v3/quote/" + symbols_str + "?apikey=" + _api_key;
            break;
    }
    _api_usage_count++;

    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure();

    if (http.begin(client, url)) {
        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            JsonDocument filter;
            filter[0]["symbol"] = true;
            filter[0]["name"] = true;
            filter[0]["price"] = true;
            filter[0]["changesPercentage"] = true;
            filter[0]["dayLow"] = true;
            filter[0]["dayHigh"] = true;
            filter[0]["volume"] = true;

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));

            if (error == DeserializationError::Ok) {
                parseJsonResponse(doc, type);
            } else {
                Log_printf(LOG_LEVEL_ERROR, "Failed to parse JSON for %s: %s", symbols_str.c_str(), error.c_str());
            }
        } else {
            Log_printf(LOG_LEVEL_ERROR, "HTTP request failed for %s with code %d", symbols_str.c_str(), httpCode);
        }
        http.end();
    } else {
        Log_printf(LOG_LEVEL_ERROR, "Failed to begin HTTP client for %s", symbols_str.c_str());
    }
}

void StockManager::parseJsonResponse(JsonDocument& doc, AssetType type) {
    JsonArray array = doc.as<JsonArray>();
    if (array.isNull()) {
        Log_printf(LOG_LEVEL_ERROR, "Failed to parse JSON response: not an array.");
        return;
    }

    for (JsonObject quote : array) {
        String symbol = quote["symbol"];
        if (symbol.isEmpty()) {
            continue;
        }

        auto it = std::find_if(_assets.begin(), _assets.end(), [&](const Asset& asset) {
            return asset.symbol.equalsIgnoreCase(symbol);
        });

        if (it != _assets.end()) {
            it->price = quote["price"].as<float>();
            it->change_percent = quote["changesPercentage"].as<float>();
            it->day_high = quote["dayHigh"].as<float>();
            it->day_low = quote["dayLow"].as<float>();
            it->volume = quote["volume"].as<unsigned long>();
            it->name = quote["name"].as<String>();
            it->last_update = millis();
            it->data_valid = true;
            Log_printf(LOG_LEVEL_INFO, "Updated data for %s", symbol.c_str());
        } else {
            Log_printf(LOG_LEVEL_WARN, "Received data for untracked symbol: %s", symbol.c_str());
        }
    }
}

void fetchStockDataBatchTask(void* p) {
    StockFetchParams* params = (StockFetchParams*)p;
    params->manager->fetchBatchData(params->symbols, params->type);

    xSemaphoreTake(params->manager->_task_mutex, portMAX_DELAY);
    params->manager->_running_tasks -= 1;
    if (params->manager->_running_tasks == 0) {
        params->manager->_is_fetching = false;
        Log_printf(LOG_LEVEL_INFO, "All stock fetch tasks complete.");
    }
    xSemaphoreGive(params->manager->_task_mutex);

    delete params;
    vTaskDelete(NULL);
}

String StockManager::getMarqueeLine() {
    if (!_enabled) {
        return "";
    }

    if (!isMarketOpen()) {
        return "MARKET CLOSED";
    }

    if (_assets.empty()) {
        return "ADD ASSETS IN UI";
    }

    if (_current_asset_index >= _assets.size()) {
        _current_asset_index = 0;
    }

    const Asset& current_asset = _assets[_current_asset_index];

    if (!current_asset.data_valid) {
        return current_asset.symbol + " NO DATA";
    }

    char buffer[50];
    switch (_current_page_index) {
        case 0: // Price and Percentage Change
            snprintf(buffer, sizeof(buffer), "%s %.2f %.2f%%",
                     current_asset.symbol.c_str(),
                     current_asset.price,
                     current_asset.change_percent);
            break;
        case 1: // Day's High and Low
            snprintf(buffer, sizeof(buffer), "%s H:%.2f L:%.2f",
                     current_asset.symbol.c_str(),
                     current_asset.day_high,
                     current_asset.day_low);
            break;
        case 2: // Trading Volume
            snprintf(buffer, sizeof(buffer), "%s VOL: %lu",
                     current_asset.symbol.c_str(),
                     current_asset.volume);
            break;
        default:
            _current_page_index = 0;
            return getMarqueeLine();
    }

    return String(buffer);
}

void StockManager::nextPage() {
    if (_assets.empty()) return;

    _current_page_index++;
    if (_current_page_index > 2) { // 3 pages: Price, High/Low, Volume
        _current_page_index = 0;
        _current_asset_index++;
        if (_current_asset_index >= _assets.size()) {
            _current_asset_index = 0;
        }
    }
    Log_printf(LOG_LEVEL_INFO, "Stock Ticker: Next Page (Asset: %d, Page: %d)", _current_asset_index, _current_page_index);
}

void StockManager::previousPage() {
    if (_assets.empty()) return;

    _current_page_index--;
    if (_current_page_index < 0) {
        _current_page_index = 2;
        _current_asset_index--;
        if (_current_asset_index < 0) {
            _current_asset_index = _assets.size() - 1;
        }
    }
    Log_printf(LOG_LEVEL_INFO, "Stock Ticker: Previous Page (Asset: %d, Page: %d)", _current_asset_index, _current_page_index);
}

void StockManager::setApiKey(const String& key) {
    _api_key = key;
}

void StockManager::setRefreshInterval(unsigned long interval) {
    _refresh_interval_ms = interval * 60 * 1000;
}

void StockManager::setEnabled(bool enabled) {
    _enabled = enabled;
    if (_enabled) {
        _last_fetch_time = 0; // Force fetch on enable
    }
}

int StockManager::getApiUsage() const {
    return _api_usage_count;
}

bool StockManager::isMarketOpen() const {
    // For now, we'll assume crypto is always open and stocks have specific hours.
    // This needs to be implemented with proper time zone handling.
    bool stock_market_open = isStockMarketOpen();
    bool crypto_market_open = isCryptoMarketOpen();

    // If any configured asset is in an open market, we consider the "market" open.
    for (const auto& asset : _assets) {
        if (asset.type == STOCK && stock_market_open) return true;
        if (asset.type == CRYPTO && crypto_market_open) return true;
        if (asset.type == INDEX && stock_market_open) return true; // Indices follow stock market hours
    }
    return false;
}

AssetType StockManager::getAssetType(const String& symbol) {
    // Simple heuristic to determine asset type. This will be improved.
    // For now, we assume anything with a "-" is crypto, anything with "^" is index.
    if (symbol.indexOf('-') != -1) {
        return CRYPTO;
    }
    if (symbol.startsWith("^")) {
        return INDEX;
    }
    return STOCK;
}

bool StockManager::isStockMarketOpen() const {
    if (!timeSynchronized) return false;

    // The market timezone is Eastern Time (ET)
    // "EST5EDT,M3.2.0,M11.1.0" is the POSIX string for US Eastern Time
    setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    tzset();

    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);

    // Market is open Monday (1) to Friday (5)
    if (timeinfo->tm_wday < 1 || timeinfo->tm_wday > 5) {
        return false;
    }

    // Market hours are 9:30 AM to 4:00 PM
    int current_minutes = timeinfo->tm_hour * 60 + timeinfo->tm_min;
    int market_open_minutes = 9 * 60 + 30;
    int market_close_minutes = 16 * 60;

    if (current_minutes >= market_open_minutes && current_minutes < market_close_minutes) {
        return true;
    }

    return false;
}

bool StockManager::isCryptoMarketOpen() const {
    return true; // Crypto market is 24/7
}
