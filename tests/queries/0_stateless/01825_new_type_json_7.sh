#!/usr/bin/env bash
# Tags: no-fasttest

CURDIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=../shell_config.sh
. "$CURDIR"/../shell_config.sh

$CLICKHOUSE_CLIENT -q "DROP TABLE IF EXISTS t_json_7;"

$CLICKHOUSE_CLIENT -q "CREATE TABLE t_json_7 (data JSON) ENGINE = MergeTree ORDER BY tuple()" --enable_json_type 1

cat <<EOF | $CLICKHOUSE_CLIENT -q "INSERT INTO t_json_7 FORMAT JSONAsObject"
{
    "key": "v1",
    "categories": null
}
{
    "key": "v2",
    "categories": ["foo", "bar"]
}
{
    "key": "v3",
    "categories": null
}
EOF

$CLICKHOUSE_CLIENT -q "SELECT DISTINCT arrayJoin(JSONAllPathsWithTypes(data)) as path FROM t_json_7 order by path;"
$CLICKHOUSE_CLIENT -q "SELECT data.key, data.categories FROM t_json_7 ORDER BY data.key" --allow_suspicious_types_in_order_by 1

$CLICKHOUSE_CLIENT -q "DROP TABLE t_json_7;"
