-- Test: Security and Access Control
-- Tests row-level security, column-level security, and policy management

-- Test 1: Create Users and Roles
CREATE ROLE admin_role;
CREATE ROLE analyst_role;
CREATE ROLE viewer_role;

CREATE USER alice WITH PASSWORD 'secure123' IN ROLE admin_role;
CREATE USER bob WITH PASSWORD 'secure456' IN ROLE analyst_role;
CREATE USER charlie WITH PASSWORD 'secure789' IN ROLE viewer_role;

-- Test 2: Grant Basic Privileges
GRANT ALL PRIVILEGES ON ALL TABLES IN SCHEMA public TO admin_role;
GRANT SELECT, INSERT, UPDATE ON TABLE users, orders TO analyst_role;
GRANT SELECT ON TABLE users, orders, products TO viewer_role;

-- Test 3: Row-Level Security (RLS)
CREATE TABLE sensitive_data (
    id SERIAL PRIMARY KEY,
    owner_id INT,
    department TEXT,
    data_value TEXT,
    classification TEXT CHECK (classification IN ('public', 'internal', 'confidential', 'secret'))
);

-- Enable RLS
ALTER TABLE sensitive_data ENABLE ROW LEVEL SECURITY;

-- Create RLS Policies
CREATE POLICY view_own_data ON sensitive_data
    FOR SELECT
    USING (owner_id = current_user_id() OR current_user_role() = 'admin_role');

CREATE POLICY view_department_data ON sensitive_data
    FOR SELECT
    USING (department = current_user_department());

CREATE POLICY classification_policy ON sensitive_data
    FOR SELECT
    USING (
        CASE classification
            WHEN 'public' THEN true
            WHEN 'internal' THEN current_user_role() IN ('admin_role', 'analyst_role', 'viewer_role')
            WHEN 'confidential' THEN current_user_role() IN ('admin_role', 'analyst_role')
            WHEN 'secret' THEN current_user_role() = 'admin_role'
        END
    );

-- Insert test data
INSERT INTO sensitive_data (owner_id, department, data_value, classification) VALUES
    (1, 'engineering', 'public_data_1', 'public'),
    (1, 'engineering', 'internal_data_1', 'internal'),
    (2, 'sales', 'confidential_data_1', 'confidential'),
    (3, 'finance', 'secret_data_1', 'secret');

-- Test 4: Column-Level Security
CREATE TABLE employee_records (
    emp_id SERIAL PRIMARY KEY,
    name TEXT,
    email TEXT,
    salary DECIMAL,
    ssn TEXT,
    performance_rating INT
);

-- Grant column-specific privileges
GRANT SELECT (emp_id, name, email) ON employee_records TO viewer_role;
GRANT SELECT (emp_id, name, email, performance_rating) ON employee_records TO analyst_role;
GRANT SELECT ON employee_records TO admin_role;

-- Test 5: Dynamic Data Masking
CREATE OR REPLACE FUNCTION mask_ssn(ssn TEXT)
RETURNS TEXT AS $$
BEGIN
    IF current_user_role() = 'admin_role' THEN
        RETURN ssn;
    ELSE
        RETURN 'XXX-XX-' || RIGHT(ssn, 4);
    END IF;
END;
$$ LANGUAGE plpgsql;

CREATE VIEW employee_records_masked AS
SELECT
    emp_id,
    name,
    email,
    CASE
        WHEN current_user_role() IN ('admin_role', 'analyst_role') THEN salary
        ELSE NULL
    END as salary,
    mask_ssn(ssn) as ssn,
    performance_rating
FROM employee_records;

-- Test 6: Audit Logging
CREATE TABLE audit_log (
    log_id SERIAL PRIMARY KEY,
    table_name TEXT,
    operation TEXT,
    user_name TEXT DEFAULT CURRENT_USER,
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    old_data JSONB,
    new_data JSONB
);

-- Create audit trigger
CREATE OR REPLACE FUNCTION audit_trigger_function()
RETURNS TRIGGER AS $$
BEGIN
    INSERT INTO audit_log (table_name, operation, old_data, new_data)
    VALUES (
        TG_TABLE_NAME,
        TG_OP,
        CASE WHEN TG_OP IN ('UPDATE', 'DELETE') THEN row_to_json(OLD) ELSE NULL END,
        CASE WHEN TG_OP IN ('INSERT', 'UPDATE') THEN row_to_json(NEW) ELSE NULL END
    );
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER audit_users
    AFTER INSERT OR UPDATE OR DELETE ON users
    FOR EACH ROW EXECUTE FUNCTION audit_trigger_function();

-- Test 7: Encryption at Rest
CREATE TABLE encrypted_data (
    id SERIAL PRIMARY KEY,
    encrypted_column BYTEA,
    metadata JSONB
) WITH (encryption_key = 'master_key_id');

-- Encrypt data before insert
INSERT INTO encrypted_data (encrypted_column, metadata)
VALUES (
    pgp_sym_encrypt('sensitive information', 'encryption_password'),
    '{"encrypted": true, "algorithm": "AES-256"}'
);

-- Decrypt data on select
SELECT
    id,
    pgp_sym_decrypt(encrypted_column, 'encryption_password') as decrypted_value,
    metadata
FROM encrypted_data;

-- Test 8: Policy for Differential Privacy
CREATE POLICY dp_policy ON users
    FOR SELECT
    USING (true)
    WITH CHECK (
        CASE
            WHEN aggregate_query() THEN dp_noise_enabled()
            ELSE true
        END
    );

-- Test 9: Time-based Access Control
CREATE POLICY time_based_access ON orders
    FOR SELECT
    USING (
        CURRENT_TIME BETWEEN TIME '09:00:00' AND TIME '17:00:00'
        OR current_user_role() = 'admin_role'
    );

-- Test 10: IP-based Access Control
CREATE TABLE access_whitelist (
    user_name TEXT PRIMARY KEY,
    allowed_ips INET[]
);

INSERT INTO access_whitelist VALUES
    ('alice', ARRAY['192.168.1.0/24'::inet, '10.0.0.0/8'::inet]),
    ('bob', ARRAY['192.168.1.100'::inet]);

CREATE POLICY ip_restriction ON sensitive_data
    FOR ALL
    USING (
        EXISTS (
            SELECT 1 FROM access_whitelist
            WHERE user_name = CURRENT_USER
            AND inet_client_addr() = ANY(allowed_ips)
        )
    );

-- Test 11: Hierarchical Access Control
CREATE TABLE org_hierarchy (
    emp_id INT PRIMARY KEY,
    manager_id INT REFERENCES org_hierarchy(emp_id),
    level INT
);

CREATE POLICY hierarchical_access ON employee_records
    FOR SELECT
    USING (
        emp_id = current_user_id()
        OR EXISTS (
            SELECT 1 FROM org_hierarchy h1
            JOIN org_hierarchy h2 ON h1.emp_id = current_user_id()
            WHERE h2.emp_id = employee_records.emp_id
            AND h2.level > h1.level
            AND h1.emp_id IN (
                WITH RECURSIVE managers AS (
                    SELECT emp_id, manager_id FROM org_hierarchy WHERE emp_id = h2.emp_id
                    UNION ALL
                    SELECT h.emp_id, h.manager_id
                    FROM org_hierarchy h
                    JOIN managers m ON h.emp_id = m.manager_id
                )
                SELECT emp_id FROM managers
            )
        )
    );

-- Test 12: Revoke Privileges
REVOKE INSERT ON orders FROM analyst_role;
REVOKE ALL ON products FROM viewer_role;
GRANT SELECT (id, name) ON products TO viewer_role;

-- Test 13: Default Privileges
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT ON TABLES TO viewer_role;
ALTER DEFAULT PRIVILEGES IN SCHEMA public
    GRANT SELECT, INSERT, UPDATE ON TABLES TO analyst_role;

-- Test 14: Security Labels
SECURITY LABEL FOR privacy_level ON TABLE users IS 'medium';
SECURITY LABEL FOR privacy_level ON COLUMN users.email IS 'high';
SECURITY LABEL FOR privacy_level ON COLUMN users.ssn IS 'critical';

-- Test 15: Connection Limits
ALTER ROLE viewer_role CONNECTION LIMIT 5;
ALTER ROLE analyst_role CONNECTION LIMIT 10;
ALTER ROLE admin_role CONNECTION LIMIT -1; -- unlimited

-- Cleanup
DROP POLICY IF EXISTS view_own_data ON sensitive_data;
DROP POLICY IF EXISTS view_department_data ON sensitive_data;
DROP POLICY IF EXISTS classification_policy ON sensitive_data;
DROP POLICY IF EXISTS dp_policy ON users;
DROP POLICY IF EXISTS time_based_access ON orders;
DROP POLICY IF EXISTS ip_restriction ON sensitive_data;
DROP POLICY IF EXISTS hierarchical_access ON employee_records;
DROP TRIGGER IF EXISTS audit_users ON users;
DROP FUNCTION IF EXISTS audit_trigger_function();
DROP FUNCTION IF EXISTS mask_ssn(TEXT);
DROP VIEW IF EXISTS employee_records_masked;
DROP TABLE IF EXISTS sensitive_data CASCADE;
DROP TABLE IF EXISTS employee_records CASCADE;
DROP TABLE IF EXISTS audit_log CASCADE;
DROP TABLE IF EXISTS encrypted_data CASCADE;
DROP TABLE IF EXISTS access_whitelist CASCADE;
DROP TABLE IF EXISTS org_hierarchy CASCADE;
DROP ROLE IF EXISTS admin_role;
DROP ROLE IF EXISTS analyst_role;
DROP ROLE IF EXISTS viewer_role;
DROP USER IF EXISTS alice;
DROP USER IF EXISTS bob;
DROP USER IF EXISTS charlie;
EXIT;