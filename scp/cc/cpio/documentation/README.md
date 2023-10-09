# Generate documentation for CMRT SDK

In outside of the scp folder, run the following command:

        docker run --rm   -v $(pwd)/doc:/out   -v $(pwd)/scp/cc/public/cpio/proto:/protos/cc/public/cpio/proto -v $(pwd)/scp/cc/core/common/proto:/protos/cc/core/common/proto  -v $(pwd)/scp/cc/cpio/documentation:/documentation pseudomuto/protoc-gen-doc --doc_opt=documentation/html_template.tmpl,sdk.html cc/public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.proto cc/public/cpio/proto/blob_storage_service/v1/blob_storage_service.proto cc/public/cpio/proto/crypto_service/v1/crypto_service.proto cc/public/cpio/proto/instance_service/v1/instance_service.proto cc/public/cpio/proto/job_service/v1/job_service.proto cc/public/cpio/proto/metric_service/v1/metric_service.proto cc/public/cpio/proto/nosql_database_service/v1/nosql_database_service.proto cc/public/cpio/proto/parameter_service/v1/parameter_service.proto cc/public/cpio/proto/private_key_service/v1/private_key_service.proto cc/public/cpio/proto/public_key_service/v1/public_key_service.proto cc/public/cpio/proto/queue_service/v1/queue_service.proto cc/core/common/proto/common.proto

The services are listed in the above command in alphabetical order. When adding more services,
please also insert them in order.
