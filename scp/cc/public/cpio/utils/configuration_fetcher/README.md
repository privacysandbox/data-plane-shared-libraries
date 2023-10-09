# ConfigurationFetcher

The helper is to get configurations stored in cloud (ParameterStore in AWS, or SecretManager in
GCP). If the customer also uses our terraform deploy script, it assumes the configurations are
stored in cloud with the key format to be scp-{environment_name}-{parameter_name} and the customer
can use this helper to fetch all kinds of configurations without knowing the parameter names.

For the configurations not deployed through our terraform script, this helper also provider an
interface to GetParameterByName, and the customer can specify the parameter name by themselves.
