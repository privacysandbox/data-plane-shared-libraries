# Metric Aggregation Utils

One call to the MetricClient will result in one request to cloud. For high-QPS traffics, to avoid
exceeding the cloud quota, we'd better pre-aggregate the metrics before send them to MetricClient.
Here are some help utils to pre-aggregate metrics.

## AggregateMetric

Keep counting the events of different types in a period. The counters are perioadically pushed to
cloud.

See the example usage [here](/cc/core/http2_server/src/http2_server.cc).

## SimpleMetric

A simple wrapper of MetricClient, and it does not pre-aggregate metrics but to keep a record of
pre-defined metric namespace and labels for convenience.

## MetricInstanceFactory

A MetricInstanceFactory makes it simple to create AggregateMetric instances and SimpleMetric
instances for system monitoring.
