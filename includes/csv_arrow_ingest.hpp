#include <arrow/csv/api.h>
#include "common.hpp"

std::shared_ptr<arrow::Table> read_encrypted_csv(const IngestProperties &props);

std::shared_ptr<arrow::Table> read_unencrypted_csv(const IngestProperties &props);