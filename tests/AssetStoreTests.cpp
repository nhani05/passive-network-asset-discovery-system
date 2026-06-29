#include "asset/AssetStore.hpp"

#include <iostream>
#include <string>
#include <utility>

namespace {

using asset_discovery::asset::AssetStore;
using asset_discovery::asset::timestampLess;
using asset_discovery::parser::AssetObservation;
using asset_discovery::parser::ObservationSource;
using asset_discovery::parser::ObservationTimestamp;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "THẤT BẠI: " << message << "\n";
        ++failures;
    }
}

AssetObservation arpObservation(
    std::string macAddress,
    std::string ipAddress,
    ObservationTimestamp timestamp)
{
    AssetObservation observation;
    observation.macAddress = std::move(macAddress);
    observation.ipAddress = std::move(ipAddress);
    observation.source = ObservationSource::Arp;
    observation.timestamp = timestamp;
    return observation;
}

void createsNewAsset()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:AC:11:00:02", "192.168.1.10", {10, 100}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "observation đầu tiên phải tạo một asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().macAddress == "02:42:ac:11:00:02", "MAC asset phải được chuẩn hóa");
    expect(assets.front().ipAddresses.count("192.168.1.10") == 1, "asset phải chứa IP từ observation");
    expect(assets.front().firstSeen.seconds == 10, "giây first_seen phải đến từ observation đầu tiên");
    expect(assets.front().lastSeen.seconds == 10, "giây last_seen phải đến từ observation đầu tiên");
    expect(assets.front().sources.count(ObservationSource::Arp) == 1, "asset phải chứa source ARP");
}

void repeatedObservationUpdatesLastSeen()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {10, 100}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {12, 200}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "cùng MAC vẫn phải là một asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().firstSeen.seconds == 10, "first_seen phải giữ observation sớm nhất");
    expect(assets.front().firstSeen.microseconds == 100, "micro giây first_seen phải giữ observation sớm nhất");
    expect(assets.front().lastSeen.seconds == 12, "last_seen phải cập nhật theo observation mới nhất");
    expect(assets.front().lastSeen.microseconds == 200, "micro giây last_seen phải cập nhật theo observation mới nhất");
}

void outOfOrderTimestampsPreserveBounds()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {20, 0}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {10, 999999}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {25, 1}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "observation lệch thứ tự phải được gộp vào một asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().firstSeen.seconds == 10, "first_seen lệch thứ tự phải là giây sớm nhất");
    expect(assets.front().firstSeen.microseconds == 999999, "first_seen lệch thứ tự phải giữ micro giây sớm nhất");
    expect(assets.front().lastSeen.seconds == 25, "last_seen lệch thứ tự phải là giây mới nhất");
    expect(assets.front().lastSeen.microseconds == 1, "last_seen lệch thứ tự phải giữ micro giây mới nhất");
}

void mergesDistinctIpAddresses()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {10, 0}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.11", {11, 0}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "IP mới của cùng MAC không được tạo asset khác");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().ipAddresses.size() == 2, "asset phải chứa cả hai địa chỉ IP khác nhau");
    expect(assets.front().ipAddresses.count("192.168.1.10") == 1, "asset phải giữ IP ban đầu");
    expect(assets.front().ipAddresses.count("192.168.1.11") == 1, "asset phải thêm IP mới");
}

void ignoresDuplicateIpAddress()
{
    AssetStore store;
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {10, 0}));
    store.applyObservation(arpObservation("02:42:ac:11:00:02", "192.168.1.10", {11, 0}));

    const auto assets = store.assets();

    expect(assets.size() == 1, "observation IP trùng lặp vẫn phải là một asset");
    if (assets.empty()) {
        return;
    }

    expect(assets.front().ipAddresses.size() == 1, "IP trùng lặp không được lưu hai lần");
}

void comparesTimestampsBySecondsThenMicroseconds()
{
    expect(timestampLess({1, 0}, {1, 1}), "so sánh timestamp phải dùng micro giây trong cùng một giây");
    expect(timestampLess({1, 999999}, {2, 0}), "so sánh timestamp phải ưu tiên giây trước");
    expect(!timestampLess({2, 0}, {1, 999999}), "giây muộn hơn không được nhỏ hơn giây sớm hơn");
}

} // namespace

int main()
{
    createsNewAsset();
    repeatedObservationUpdatesLastSeen();
    outOfOrderTimestampsPreserveBounds();
    mergesDistinctIpAddresses();
    ignoresDuplicateIpAddress();
    comparesTimestampsBySecondsThenMicroseconds();

    if (failures > 0) {
        std::cerr << failures << " kỳ vọng test asset store thất bại\n";
        return 1;
    }

    return 0;
}
