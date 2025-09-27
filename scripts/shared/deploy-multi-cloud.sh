#!/bin/bash
set -e

CLOUD_PROVIDER="${1:-aws}"
ENVIRONMENT="${2:-dev}"
ACTION="${3:-deploy}"

deploy_aws() {
    cd aws
    terraform init
    terraform apply -var-file="../environments/${ENVIRONMENT}/aws.tfvars" -auto-approve
}

deploy_azure() {
    cd azure
    terraform init
    terraform apply -var-file="../environments/${ENVIRONMENT}/azure.tfvars" -auto-approve
}

deploy_gcp() {
    cd gcp
    terraform init
    terraform apply -var-file="../environments/${ENVIRONMENT}/gcp.tfvars" -auto-approve
}

case "$CLOUD_PROVIDER" in
    aws) deploy_aws ;;
    azure) deploy_azure ;;
    gcp) deploy_gcp ;;
    all)
        deploy_aws &
        deploy_azure &
        deploy_gcp &
        wait
        ;;
    *) echo "Usage: $0 {aws|azure|gcp|all} {dev|staging|prod} {deploy|destroy}"; exit 1 ;;
esac

echo "Deployment to $CLOUD_PROVIDER ($ENVIRONMENT) completed!"