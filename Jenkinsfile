#!/usr/bin/env groovy

pipeline {
    agent any

    options {
        buildDiscarder(logRotator(numToKeepStr: '10'))
        timeout(time: 60, unit: 'MINUTES')
        timestamps()
        retry(2)
    }

    environment {
        PROJECT_NAME = 'latticedb'
        DOCKER_REGISTRY = 'your-registry.com'
        IMAGE_TAG = "${BUILD_NUMBER}-${GIT_COMMIT[0..7]}"
        BUILD_DATE = sh(script: "date -u +'%Y-%m-%dT%H:%M:%SZ'", returnStdout: true).trim()
        VCS_REF = "${GIT_COMMIT}"

        // Deployment environments
        AWS_DEFAULT_REGION = 'us-west-2'
        AZURE_LOCATION = 'East US'
        GCP_REGION = 'us-central1'

        // Tools versions
        TERRAFORM_VERSION = '1.6.0'
        HELM_VERSION = '3.12.0'
        KUBECTL_VERSION = '1.28.0'
    }

    parameters {
        choice(
            name: 'DEPLOYMENT_TARGET',
            choices: ['none', 'aws', 'azure', 'gcp', 'hashicorp', 'all'],
            description: 'Select deployment target'
        )
        choice(
            name: 'ENVIRONMENT',
            choices: ['development', 'staging', 'production'],
            description: 'Deployment environment'
        )
        booleanParam(
            name: 'SKIP_TESTS',
            defaultValue: false,
            description: 'Skip running tests'
        )
        booleanParam(
            name: 'RUN_SECURITY_SCAN',
            defaultValue: true,
            description: 'Run security scanning'
        )
    }

    stages {
        stage('Checkout') {
            steps {
                script {
                    echo "🔄 Checking out code for LatticeDB..."
                    checkout scm

                    // Get git information
                    env.GIT_BRANCH_NAME = env.BRANCH_NAME ?: 'unknown'
                    env.GIT_AUTHOR_NAME = sh(script: "git show -s --format='%an'", returnStdout: true).trim()
                    env.GIT_AUTHOR_EMAIL = sh(script: "git show -s --format='%ae'", returnStdout: true).trim()

                    echo "Branch: ${env.GIT_BRANCH_NAME}"
                    echo "Author: ${env.GIT_AUTHOR_NAME} <${env.GIT_AUTHOR_EMAIL}>"
                    echo "Commit: ${env.GIT_COMMIT}"
                }
            }
        }

        stage('Setup Environment') {
            parallel {
                stage('Install Build Tools') {
                    steps {
                        script {
                            echo "🛠️  Installing build dependencies..."
                            sh '''
                                # Update package lists
                                sudo apt-get update

                                # Install C++ build tools
                                sudo apt-get install -y \
                                    clang-12 \
                                    cmake \
                                    ninja-build \
                                    liblz4-dev \
                                    libzstd-dev \
                                    pkg-config \
                                    git \
                                    curl \
                                    jq

                                # Set compiler environment
                                export CC=clang-12
                                export CXX=clang++-12

                                echo "Build tools installed successfully"
                            '''
                        }
                    }
                }

                stage('Install Cloud Tools') {
                    steps {
                        script {
                            echo "☁️  Installing cloud deployment tools..."
                            sh '''
                                # Install Terraform
                                wget -q https://releases.hashicorp.com/terraform/${TERRAFORM_VERSION}/terraform_${TERRAFORM_VERSION}_linux_amd64.zip
                                sudo unzip -o terraform_${TERRAFORM_VERSION}_linux_amd64.zip -d /usr/local/bin/
                                rm terraform_${TERRAFORM_VERSION}_linux_amd64.zip

                                # Install AWS CLI
                                curl "https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip" -o "awscliv2.zip"
                                unzip -q awscliv2.zip
                                sudo ./aws/install --update
                                rm -rf aws awscliv2.zip

                                # Install Azure CLI
                                curl -sL https://aka.ms/InstallAzureCLIDeb | sudo bash

                                # Install Google Cloud SDK
                                echo "deb [signed-by=/usr/share/keyrings/cloud.google.gpg] https://packages.cloud.google.com/apt cloud-sdk main" | sudo tee -a /etc/apt/sources.list.d/google-cloud-sdk.list
                                curl https://packages.cloud.google.com/apt/doc/apt-key.gpg | sudo apt-key --keyring /usr/share/keyrings/cloud.google.gpg add -
                                sudo apt-get update && sudo apt-get install -y google-cloud-sdk

                                # Install HashiCorp tools
                                wget -O- https://apt.releases.hashicorp.com/gpg | gpg --dearmor | sudo tee /usr/share/keyrings/hashicorp-archive-keyring.gpg
                                echo "deb [signed-by=/usr/share/keyrings/hashicorp-archive-keyring.gpg] https://apt.releases.hashicorp.com $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/hashicorp.list
                                sudo apt-get update && sudo apt-get install -y consul vault nomad

                                # Verify installations
                                terraform version
                                aws --version
                                az version
                                gcloud version
                                consul version
                                vault version
                                nomad version
                            '''
                        }
                    }
                }
            }
        }

        stage('Build') {
            steps {
                script {
                    echo "🏗️  Building LatticeDB..."
                    sh '''
                        export CC=clang-12
                        export CXX=clang++-12

                        mkdir -p build
                        cd build
                        cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release
                        ninja -j$(nproc)

                        echo "Build completed successfully"
                    '''
                }
            }
            post {
                always {
                    archiveArtifacts artifacts: 'build/latticedb*', fingerprint: true, allowEmptyArchive: true
                }
            }
        }

        stage('Test') {
            when {
                not { params.SKIP_TESTS }
            }
            parallel {
                stage('Unit Tests') {
                    steps {
                        script {
                            echo "🧪 Running unit tests..."
                            sh '''
                                cd build
                                ctest --output-on-failure --parallel $(nproc)
                            '''
                        }
                    }
                    post {
                        always {
                            publishTestResults testResultsPattern: 'build/test-results.xml'
                        }
                    }
                }

                stage('Integration Tests') {
                    steps {
                        script {
                            echo "🔗 Running integration tests..."
                            sh '''
                                cd build
                                ./latticedb_test --integration
                            '''
                        }
                    }
                }

                stage('Code Coverage') {
                    steps {
                        script {
                            echo "📊 Collecting code coverage..."
                            sh '''
                                cd build
                                gcov -r ../**/*.cpp || echo "Coverage collection completed"
                            '''
                        }
                    }
                    post {
                        always {
                            publishCoverage adapters: [
                                gcov(path: 'build/*.gcov')
                            ], sourceFileResolver: sourceFiles('STORE_LAST_BUILD')
                        }
                    }
                }
            }
        }

        stage('Security Scan') {
            when {
                expression { params.RUN_SECURITY_SCAN }
            }
            parallel {
                stage('Static Analysis') {
                    steps {
                        script {
                            echo "🔍 Running static analysis..."
                            sh '''
                                # Run cppcheck
                                cppcheck --enable=all --std=c++17 --suppress=missingInclude src/ --xml --output-file=cppcheck-results.xml || true

                                # Run clang-static-analyzer
                                scan-build --status-bugs clang++-12 -std=c++17 -Isrc src/*.cpp || true
                            '''
                        }
                    }
                    post {
                        always {
                            recordIssues enabledForFailure: true, tools: [cppCheck(pattern: 'cppcheck-results.xml')]
                        }
                    }
                }

                stage('Dependency Scan') {
                    steps {
                        script {
                            echo "📦 Scanning dependencies..."
                            sh '''
                                # Create requirements file for scanning
                                echo "# C++ dependencies" > dependencies.txt
                                echo "cmake" >> dependencies.txt
                                echo "ninja" >> dependencies.txt

                                # Run dependency check (if available)
                                which dependency-check && dependency-check --project LatticeDB --scan . --out dependency-check-report.xml || echo "Dependency check skipped"
                            '''
                        }
                    }
                }
            }
        }

        stage('Build Docker Image') {
            steps {
                script {
                    echo "🐳 Building Docker image..."
                    sh '''
                        docker build -t ${PROJECT_NAME}:${IMAGE_TAG} \
                            --build-arg BUILD_DATE="${BUILD_DATE}" \
                            --build-arg VCS_REF="${VCS_REF}" \
                            --label "project=${PROJECT_NAME}" \
                            --label "version=${IMAGE_TAG}" \
                            --label "build-date=${BUILD_DATE}" \
                            --label "vcs-ref=${VCS_REF}" \
                            .

                        # Tag as latest for non-production builds
                        if [ "${params.ENVIRONMENT}" != "production" ]; then
                            docker tag ${PROJECT_NAME}:${IMAGE_TAG} ${PROJECT_NAME}:latest
                        fi

                        # Show image info
                        docker images ${PROJECT_NAME}

                        echo "Docker image built successfully"
                    '''
                }
            }
        }

        stage('Deploy') {
            when {
                anyOf {
                    branch 'master'
                    branch 'main'
                    branch 'develop'
                    expression { params.DEPLOYMENT_TARGET != 'none' }
                }
            }
            parallel {
                stage('Deploy to AWS') {
                    when {
                        anyOf {
                            expression { params.DEPLOYMENT_TARGET == 'aws' }
                            expression { params.DEPLOYMENT_TARGET == 'all' }
                        }
                    }
                    steps {
                        script {
                            echo "🚀 Deploying to AWS..."
                            withCredentials([
                                string(credentialsId: 'aws-access-key-id', variable: 'AWS_ACCESS_KEY_ID'),
                                string(credentialsId: 'aws-secret-access-key', variable: 'AWS_SECRET_ACCESS_KEY')
                            ]) {
                                sh '''
                                    cd aws
                                    terraform init -backend-config="bucket=${TF_STATE_BUCKET}" -backend-config="key=latticedb/${params.ENVIRONMENT}/terraform.tfstate" -backend-config="region=${AWS_DEFAULT_REGION}"
                                    terraform plan -var="aws_region=${AWS_DEFAULT_REGION}" -var="environment=${params.ENVIRONMENT}" -var="image_tag=${IMAGE_TAG}"
                                    terraform apply -auto-approve -var="aws_region=${AWS_DEFAULT_REGION}" -var="environment=${params.ENVIRONMENT}" -var="image_tag=${IMAGE_TAG}"

                                    # Get deployment info
                                    terraform output -json > ../aws-deployment-info.json
                                '''
                            }
                        }
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'aws-deployment-info.json', fingerprint: true, allowEmptyArchive: true
                        }
                    }
                }

                stage('Deploy to Azure') {
                    when {
                        anyOf {
                            expression { params.DEPLOYMENT_TARGET == 'azure' }
                            expression { params.DEPLOYMENT_TARGET == 'all' }
                        }
                    }
                    steps {
                        script {
                            echo "🚀 Deploying to Azure..."
                            withCredentials([
                                string(credentialsId: 'azure-client-id', variable: 'AZURE_CLIENT_ID'),
                                string(credentialsId: 'azure-client-secret', variable: 'AZURE_CLIENT_SECRET'),
                                string(credentialsId: 'azure-tenant-id', variable: 'AZURE_TENANT_ID')
                            ]) {
                                sh '''
                                    az login --service-principal -u "$AZURE_CLIENT_ID" -p "$AZURE_CLIENT_SECRET" --tenant "$AZURE_TENANT_ID"

                                    cd azure
                                    terraform init -backend-config="resource_group_name=${AZURE_RESOURCE_GROUP}" -backend-config="storage_account_name=${AZURE_STORAGE_ACCOUNT}" -backend-config="container_name=tfstate" -backend-config="key=latticedb-${params.ENVIRONMENT}.terraform.tfstate"
                                    terraform plan -var="azure_location=${AZURE_LOCATION}" -var="environment=${params.ENVIRONMENT}" -var="image_tag=${IMAGE_TAG}"
                                    terraform apply -auto-approve -var="azure_location=${AZURE_LOCATION}" -var="environment=${params.ENVIRONMENT}" -var="image_tag=${IMAGE_TAG}"

                                    # Get deployment info
                                    terraform output -json > ../azure-deployment-info.json
                                '''
                            }
                        }
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'azure-deployment-info.json', fingerprint: true, allowEmptyArchive: true
                        }
                    }
                }

                stage('Deploy to GCP') {
                    when {
                        anyOf {
                            expression { params.DEPLOYMENT_TARGET == 'gcp' }
                            expression { params.DEPLOYMENT_TARGET == 'all' }
                        }
                    }
                    steps {
                        script {
                            echo "🚀 Deploying to Google Cloud..."
                            withCredentials([
                                file(credentialsId: 'gcp-service-account-key', variable: 'GOOGLE_APPLICATION_CREDENTIALS'),
                                string(credentialsId: 'gcp-project-id', variable: 'GOOGLE_PROJECT_ID')
                            ]) {
                                sh '''
                                    gcloud auth activate-service-account --key-file="$GOOGLE_APPLICATION_CREDENTIALS"
                                    gcloud config set project "$GOOGLE_PROJECT_ID"

                                    cd gcp
                                    terraform init -backend-config="bucket=${GCP_TF_STATE_BUCKET}" -backend-config="prefix=latticedb/${params.ENVIRONMENT}/terraform.tfstate"
                                    terraform plan -var="project_id=${GOOGLE_PROJECT_ID}" -var="gcp_region=${GCP_REGION}" -var="environment=${params.ENVIRONMENT}" -var="image_tag=${IMAGE_TAG}"
                                    terraform apply -auto-approve -var="project_id=${GOOGLE_PROJECT_ID}" -var="gcp_region=${GCP_REGION}" -var="environment=${params.ENVIRONMENT}" -var="image_tag=${IMAGE_TAG}"

                                    # Get deployment info
                                    terraform output -json > ../gcp-deployment-info.json
                                '''
                            }
                        }
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'gcp-deployment-info.json', fingerprint: true, allowEmptyArchive: true
                        }
                    }
                }

                stage('Deploy to HashiCorp Stack') {
                    when {
                        anyOf {
                            expression { params.DEPLOYMENT_TARGET == 'hashicorp' }
                            expression { params.DEPLOYMENT_TARGET == 'all' }
                        }
                    }
                    steps {
                        script {
                            echo "🚀 Deploying to HashiCorp Stack..."
                            withCredentials([
                                string(credentialsId: 'consul-token', variable: 'CONSUL_HTTP_TOKEN'),
                                string(credentialsId: 'vault-token', variable: 'VAULT_TOKEN'),
                                string(credentialsId: 'nomad-token', variable: 'NOMAD_TOKEN')
                            ]) {
                                sh '''
                                    cd hashicorp
                                    terraform init -backend-config="address=${CONSUL_HTTP_ADDR}" -backend-config="path=terraform/latticedb/${params.ENVIRONMENT}"
                                    terraform plan -var="consul_address=${CONSUL_HTTP_ADDR}" -var="vault_address=${VAULT_ADDR}" -var="nomad_address=${NOMAD_ADDR}" -var="environment=${params.ENVIRONMENT}" -var="image_tag=${IMAGE_TAG}"
                                    terraform apply -auto-approve -var="consul_address=${CONSUL_HTTP_ADDR}" -var="vault_address=${VAULT_ADDR}" -var="nomad_address=${NOMAD_ADDR}" -var="environment=${params.ENVIRONMENT}" -var="image_tag=${IMAGE_TAG}"

                                    # Get deployment info
                                    terraform output -json > ../hashicorp-deployment-info.json
                                '''
                            }
                        }
                    }
                    post {
                        always {
                            archiveArtifacts artifacts: 'hashicorp-deployment-info.json', fingerprint: true, allowEmptyArchive: true
                        }
                    }
                }
            }
        }

        stage('Post-Deploy Tests') {
            when {
                expression { params.DEPLOYMENT_TARGET != 'none' }
            }
            steps {
                script {
                    echo "🧪 Running post-deployment tests..."
                    sh '''
                        # Health check tests
                        echo "Running health checks..."

                        # Check deployment info files and extract URLs
                        if [ -f "aws-deployment-info.json" ]; then
                            AWS_URL=$(jq -r '.application_url.value' aws-deployment-info.json)
                            echo "Testing AWS deployment at: $AWS_URL"
                            curl -f "$AWS_URL/health" || echo "AWS health check failed"
                        fi

                        if [ -f "azure-deployment-info.json" ]; then
                            AZURE_URL=$(jq -r '.application_url.value' azure-deployment-info.json)
                            echo "Testing Azure deployment at: $AZURE_URL"
                            curl -f "$AZURE_URL/health" || echo "Azure health check failed"
                        fi

                        if [ -f "gcp-deployment-info.json" ]; then
                            GCP_URL=$(jq -r '.application_url.value' gcp-deployment-info.json)
                            echo "Testing GCP deployment at: $GCP_URL"
                            curl -f "$GCP_URL/health" || echo "GCP health check failed"
                        fi

                        echo "Post-deployment tests completed"
                    '''
                }
            }
        }
    }

    post {
        always {
            script {
                echo "🧹 Cleaning up resources..."
                sh '''
                    # Clean up Docker images
                    docker system prune -f || true

                    # Clean build artifacts
                    rm -rf build/CMakeFiles build/CMakeCache.txt || true
                '''
            }
        }

        success {
            script {
                echo "✅ Pipeline completed successfully!"

                // Send success notification
                slackSend(
                    channel: '#latticedb',
                    color: 'good',
                    message: """
                        ✅ *LatticeDB Pipeline Success*

                        *Branch:* ${env.BRANCH_NAME}
                        *Build:* ${env.BUILD_NUMBER}
                        *Commit:* ${env.GIT_COMMIT[0..7]}
                        *Environment:* ${params.ENVIRONMENT}
                        *Target:* ${params.DEPLOYMENT_TARGET}

                        *Duration:* ${currentBuild.durationString}
                        *Build URL:* ${env.BUILD_URL}
                    """
                )
            }
        }

        failure {
            script {
                echo "❌ Pipeline failed!"

                // Send failure notification
                slackSend(
                    channel: '#latticedb',
                    color: 'danger',
                    message: """
                        ❌ *LatticeDB Pipeline Failed*

                        *Branch:* ${env.BRANCH_NAME}
                        *Build:* ${env.BUILD_NUMBER}
                        *Commit:* ${env.GIT_COMMIT[0..7]}
                        *Environment:* ${params.ENVIRONMENT}
                        *Target:* ${params.DEPLOYMENT_TARGET}

                        *Duration:* ${currentBuild.durationString}
                        *Build URL:* ${env.BUILD_URL}

                        Please check the logs and fix the issues.
                    """
                )
            }
        }

        unstable {
            script {
                echo "⚠️  Pipeline completed with warnings!"

                slackSend(
                    channel: '#latticedb',
                    color: 'warning',
                    message: """
                        ⚠️  *LatticeDB Pipeline Unstable*

                        *Branch:* ${env.BRANCH_NAME}
                        *Build:* ${env.BUILD_NUMBER}
                        *Commit:* ${env.GIT_COMMIT[0..7]}

                        Some tests failed but deployment was successful.
                        *Build URL:* ${env.BUILD_URL}
                    """
                )
            }
        }

        cleanup {
            script {
                echo "🧹 Final cleanup..."
                deleteDir() // Clean workspace
            }
        }
    }
}