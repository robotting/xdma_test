#pragma once

#include <cstdint>
#include <vector>

namespace xdma_app {

void draw_hex_dump(const std::vector<uint8_t>& data, uint64_t base_addr = 0, size_t max_rows = 0);
void draw_hex_preview(const char* title, const std::vector<uint8_t>& data, size_t max_rows = 8);

}  // namespace xdma_app
