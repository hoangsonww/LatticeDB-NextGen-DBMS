// gui/src/store.ts
import { create } from "zustand";
import { persist } from "zustand/middleware";

export type QueryResult = {
  ok: boolean;
  message: string;
  headers: string[];
  rows: any[][];
};

export type TableSchema = {
  name: string;
  columns: {
    name: string;
    type: string;
    nullable: boolean;
    primary: boolean;
  }[];
  rowCount: number;
};

type State = {
  sql: string;
  executing: boolean;
  result?: QueryResult;
  history: { ts: number; sql: string; result?: QueryResult }[];
  dpEpsilon: number;
  txAsOf?: number;
  darkMode: boolean;
  mockMode: boolean;
  tables: TableSchema[];
  favorites: string[];
  setSql: (s: string) => void;
  run: (sql?: string) => Promise<void>;
  setDp: (e: number) => void;
  setTx: (tx?: number) => void;
  toggleDarkMode: () => void;
  toggleMockMode: () => void;
  addToFavorites: (sql: string) => void;
  removeFromFavorites: (sql: string) => void;
};

// Mock database schema
const mockTables: TableSchema[] = [
  {
    name: "users",
    columns: [
      { name: "id", type: "INTEGER", nullable: false, primary: true },
      { name: "name", type: "VARCHAR(100)", nullable: false, primary: false },
      { name: "email", type: "VARCHAR(255)", nullable: false, primary: false },
      { name: "age", type: "INTEGER", nullable: true, primary: false },
      {
        name: "created_at",
        type: "TIMESTAMP",
        nullable: false,
        primary: false,
      },
      {
        name: "profile_vector",
        type: "VECTOR(4)",
        nullable: true,
        primary: false,
      },
    ],
    rowCount: 1247,
  },
  {
    name: "orders",
    columns: [
      { name: "id", type: "INTEGER", nullable: false, primary: true },
      { name: "user_id", type: "INTEGER", nullable: false, primary: false },
      {
        name: "total_amount",
        type: "DECIMAL(10,2)",
        nullable: false,
        primary: false,
      },
      { name: "status", type: "VARCHAR(50)", nullable: false, primary: false },
      {
        name: "order_date",
        type: "TIMESTAMP",
        nullable: false,
        primary: false,
      },
    ],
    rowCount: 3891,
  },
  {
    name: "products",
    columns: [
      { name: "id", type: "INTEGER", nullable: false, primary: true },
      { name: "name", type: "VARCHAR(200)", nullable: false, primary: false },
      { name: "price", type: "DECIMAL(8,2)", nullable: false, primary: false },
      { name: "category_id", type: "INTEGER", nullable: true, primary: false },
      {
        name: "embedding",
        type: "VECTOR(768)",
        nullable: true,
        primary: false,
      },
    ],
    rowCount: 567,
  },
  {
    name: "analytics_events",
    columns: [
      { name: "id", type: "BIGINT", nullable: false, primary: true },
      {
        name: "event_name",
        type: "VARCHAR(100)",
        nullable: false,
        primary: false,
      },
      { name: "user_id", type: "INTEGER", nullable: true, primary: false },
      { name: "properties", type: "JSON", nullable: true, primary: false },
      { name: "timestamp", type: "TIMESTAMP", nullable: false, primary: false },
    ],
    rowCount: 89234,
  },
];

// Extensive mock data with realistic entries
const mockUsers = [
  [
    1,
    "Alice Smith",
    "alice.smith@example.com",
    30,
    "2024-01-15 10:30:00",
    "[0.12, 0.34, 0.56, 0.78]",
  ],
  [
    2,
    "Bob Johnson",
    "bob.johnson@example.com",
    25,
    "2024-01-16 14:22:15",
    "[0.23, 0.45, 0.67, 0.89]",
  ],
  [
    3,
    "Charlie Brown",
    "charlie.brown@example.com",
    35,
    "2024-01-17 09:15:30",
    "[0.34, 0.56, 0.78, 0.12]",
  ],
  [
    4,
    "Diana Prince",
    "diana.prince@example.com",
    28,
    "2024-01-18 16:45:22",
    "[0.45, 0.67, 0.89, 0.23]",
  ],
  [
    5,
    "Eve Adams",
    "eve.adams@example.com",
    32,
    "2024-01-19 11:20:45",
    "[0.56, 0.78, 0.12, 0.34]",
  ],
  [
    6,
    "Frank Miller",
    "frank.miller@example.com",
    41,
    "2024-01-20 08:30:15",
    "[0.67, 0.89, 0.23, 0.45]",
  ],
  [
    7,
    "Grace Lee",
    "grace.lee@example.com",
    29,
    "2024-01-21 13:45:30",
    "[0.78, 0.12, 0.34, 0.56]",
  ],
  [
    8,
    "Henry Wilson",
    "henry.wilson@example.com",
    33,
    "2024-01-22 10:15:45",
    "[0.89, 0.23, 0.45, 0.67]",
  ],
  [
    9,
    "Ivy Chen",
    "ivy.chen@example.com",
    27,
    "2024-01-23 15:30:22",
    "[0.11, 0.33, 0.55, 0.77]",
  ],
  [
    10,
    "Jack Davis",
    "jack.davis@example.com",
    38,
    "2024-01-24 09:45:15",
    "[0.22, 0.44, 0.66, 0.88]",
  ],
  [
    11,
    "Kate Moore",
    "kate.moore@example.com",
    31,
    "2024-01-25 12:20:30",
    "[0.33, 0.55, 0.77, 0.11]",
  ],
  [
    12,
    "Liam Taylor",
    "liam.taylor@example.com",
    26,
    "2024-01-26 14:35:45",
    "[0.44, 0.66, 0.88, 0.22]",
  ],
  [
    13,
    "Mia Anderson",
    "mia.anderson@example.com",
    34,
    "2024-01-27 11:10:20",
    "[0.55, 0.77, 0.11, 0.33]",
  ],
  [
    14,
    "Noah Garcia",
    "noah.garcia@example.com",
    29,
    "2024-01-28 16:25:35",
    "[0.66, 0.88, 0.22, 0.44]",
  ],
  [
    15,
    "Olivia Martinez",
    "olivia.martinez@example.com",
    32,
    "2024-01-29 10:40:50",
    "[0.77, 0.11, 0.33, 0.55]",
  ],
  [
    16,
    "Paul Rodriguez",
    "paul.rodriguez@example.com",
    36,
    "2024-01-30 13:15:25",
    "[0.88, 0.22, 0.44, 0.66]",
  ],
  [
    17,
    "Quinn Thompson",
    "quinn.thompson@example.com",
    28,
    "2024-02-01 09:30:40",
    "[0.19, 0.37, 0.51, 0.73]",
  ],
  [
    18,
    "Ruby White",
    "ruby.white@example.com",
    31,
    "2024-02-02 15:45:15",
    "[0.28, 0.46, 0.62, 0.84]",
  ],
  [
    19,
    "Sam Harris",
    "sam.harris@example.com",
    37,
    "2024-02-03 12:00:30",
    "[0.37, 0.55, 0.73, 0.19]",
  ],
  [
    20,
    "Tina Clark",
    "tina.clark@example.com",
    25,
    "2024-02-04 14:20:45",
    "[0.46, 0.64, 0.82, 0.28]",
  ],
];

const mockOrders = [
  [1, 1, 299.99, "completed", "2024-01-20 14:30:15"],
  [2, 2, 149.5, "pending", "2024-01-21 10:15:30"],
  [3, 3, 89.99, "processing", "2024-01-21 16:45:20"],
  [4, 4, 199.99, "completed", "2024-01-22 09:30:45"],
  [5, 1, 75.5, "cancelled", "2024-01-22 11:20:15"],
  [6, 5, 450.0, "completed", "2024-01-23 13:45:30"],
  [7, 6, 32.99, "pending", "2024-01-23 15:10:45"],
  [8, 7, 129.99, "processing", "2024-01-24 08:25:20"],
  [9, 8, 89.99, "completed", "2024-01-24 12:40:35"],
  [10, 9, 299.99, "pending", "2024-01-25 10:55:50"],
  [11, 10, 67.5, "completed", "2024-01-25 14:20:25"],
  [12, 2, 189.99, "processing", "2024-01-26 09:15:40"],
  [13, 11, 425.75, "completed", "2024-01-26 16:30:15"],
  [14, 12, 99.99, "cancelled", "2024-01-27 11:45:30"],
  [15, 13, 159.5, "pending", "2024-01-27 13:00:45"],
  [16, 14, 379.99, "completed", "2024-01-28 10:25:20"],
  [17, 15, 49.99, "processing", "2024-01-28 15:40:35"],
  [18, 3, 229.99, "completed", "2024-01-29 12:15:50"],
  [19, 16, 89.5, "pending", "2024-01-29 14:30:25"],
  [20, 17, 399.99, "completed", "2024-01-30 11:45:40"],
];

const mockProducts = [
  [1, "Wireless Bluetooth Headphones", 129.99, 1, "[0.1, 0.2, ...]"],
  [2, "Smart Fitness Watch", 299.99, 1, "[0.2, 0.3, ...]"],
  [3, "Premium Coffee Maker", 189.99, 2, "[0.3, 0.4, ...]"],
  [4, "Ergonomic Office Chair", 449.99, 3, "[0.4, 0.5, ...]"],
  [5, "Mechanical Keyboard", 149.99, 4, "[0.5, 0.6, ...]"],
  [6, "4K Webcam", 199.99, 4, "[0.6, 0.7, ...]"],
  [7, "Portable Charger 20000mAh", 49.99, 1, "[0.7, 0.8, ...]"],
  [8, "LED Desk Lamp", 79.99, 3, "[0.8, 0.9, ...]"],
  [9, "Wireless Mouse", 69.99, 4, "[0.9, 0.1, ...]"],
  [10, "Standing Desk Converter", 299.99, 3, "[0.1, 0.9, ...]"],
  [11, "Noise Cancelling Earbuds", 179.99, 1, "[0.2, 0.8, ...]"],
  [12, "Smart Home Speaker", 99.99, 1, "[0.3, 0.7, ...]"],
  [13, "Electric Kettle", 59.99, 2, "[0.4, 0.6, ...]"],
  [14, "Yoga Mat Premium", 89.99, 5, "[0.5, 0.5, ...]"],
  [15, "Laptop Stand Adjustable", 79.99, 4, "[0.6, 0.4, ...]"],
  [16, "Air Purifier", 249.99, 6, "[0.7, 0.3, ...]"],
  [17, "Wireless Charging Pad", 39.99, 1, "[0.8, 0.2, ...]"],
  [18, "Insulated Water Bottle", 34.99, 5, "[0.9, 0.1, ...]"],
  [19, "Bluetooth Speaker Portable", 89.99, 1, "[0.1, 0.8, ...]"],
  [20, "Gaming Mouse Pad XXL", 29.99, 4, "[0.2, 0.7, ...]"],
];

const mockEvents = [
  [
    1,
    "page_view",
    1,
    '{"page": "/dashboard", "referrer": "google.com"}',
    "2024-02-01 10:15:30",
  ],
  [
    2,
    "button_click",
    1,
    '{"button_id": "purchase", "product_id": 15}',
    "2024-02-01 10:16:45",
  ],
  [
    3,
    "page_view",
    2,
    '{"page": "/products", "category": "electronics"}',
    "2024-02-01 10:20:15",
  ],
  [
    4,
    "search",
    3,
    '{"query": "wireless headphones", "results": 12}',
    "2024-02-01 10:25:30",
  ],
  [
    5,
    "add_to_cart",
    3,
    '{"product_id": 1, "quantity": 1, "price": 129.99}',
    "2024-02-01 10:26:45",
  ],
  [
    6,
    "purchase",
    4,
    '{"order_id": 16, "total": 379.99, "items": 2}',
    "2024-02-01 11:30:15",
  ],
  [
    7,
    "page_view",
    5,
    '{"page": "/profile", "section": "settings"}',
    "2024-02-01 11:45:20",
  ],
  [
    8,
    "form_submit",
    6,
    '{"form": "newsletter", "email": "frank.miller@example.com"}',
    "2024-02-01 12:15:35",
  ],
  [
    9,
    "video_play",
    7,
    '{"video_id": "product_demo_1", "duration": 120}',
    "2024-02-01 13:20:45",
  ],
  [
    10,
    "download",
    8,
    '{"file": "user_manual.pdf", "product_id": 4}',
    "2024-02-01 14:30:20",
  ],
  [
    11,
    "page_view",
    9,
    '{"page": "/checkout", "step": "payment"}',
    "2024-02-01 15:45:15",
  ],
  [
    12,
    "error",
    10,
    '{"error": "payment_failed", "code": "CARD_DECLINED"}',
    "2024-02-01 15:46:30",
  ],
  [
    13,
    "logout",
    11,
    '{"session_duration": 3600, "pages_viewed": 8}',
    "2024-02-01 16:20:45",
  ],
  [
    14,
    "login",
    12,
    '{"method": "google_oauth", "new_user": false}',
    "2024-02-01 17:15:20",
  ],
  [
    15,
    "search",
    13,
    '{"query": "office chair", "results": 5, "filter": "price_asc"}',
    "2024-02-01 18:30:35",
  ],
  [
    16,
    "page_view",
    14,
    '{"page": "/reviews", "product_id": 4}',
    "2024-02-01 19:45:50",
  ],
  [
    17,
    "review_submit",
    14,
    '{"product_id": 4, "rating": 5, "text_length": 234}',
    "2024-02-01 19:50:15",
  ],
  [
    18,
    "share",
    15,
    '{"platform": "twitter", "content": "product_link", "product_id": 2}',
    "2024-02-01 20:15:30",
  ],
  [
    19,
    "wishlist_add",
    16,
    '{"product_id": 11, "category": "electronics"}',
    "2024-02-01 21:20:45",
  ],
  [
    20,
    "chat_start",
    17,
    '{"support_topic": "shipping", "wait_time": 45}',
    "2024-02-01 22:30:20",
  ],
];

// Mock query results with extensive realistic data
const mockResults: Record<string, QueryResult> = {
  "SELECT * FROM users LIMIT 5": {
    ok: true,
    message: "5 rows returned",
    headers: ["id", "name", "email", "age", "created_at", "profile_vector"],
    rows: mockUsers.slice(0, 5),
  },
  "SELECT * FROM users LIMIT 10": {
    ok: true,
    message: "10 rows returned",
    headers: ["id", "name", "email", "age", "created_at", "profile_vector"],
    rows: mockUsers.slice(0, 10),
  },
  "SELECT * FROM users LIMIT 20": {
    ok: true,
    message: "20 rows returned",
    headers: ["id", "name", "email", "age", "created_at", "profile_vector"],
    rows: mockUsers,
  },
  "SELECT * FROM orders LIMIT 10": {
    ok: true,
    message: "10 rows returned",
    headers: ["id", "user_id", "total_amount", "status", "order_date"],
    rows: mockOrders.slice(0, 10),
  },
  "SELECT * FROM orders LIMIT 20": {
    ok: true,
    message: "20 rows returned",
    headers: ["id", "user_id", "total_amount", "status", "order_date"],
    rows: mockOrders,
  },
  "SELECT * FROM products LIMIT 10": {
    ok: true,
    message: "10 rows returned",
    headers: ["id", "name", "price", "category_id", "embedding"],
    rows: mockProducts.slice(0, 10),
  },
  "SELECT * FROM products LIMIT 15": {
    ok: true,
    message: "15 rows returned",
    headers: ["id", "name", "price", "category_id", "embedding"],
    rows: mockProducts.slice(0, 15),
  },
  "SELECT * FROM analytics_events LIMIT 10": {
    ok: true,
    message: "10 rows returned",
    headers: ["id", "event_name", "user_id", "properties", "timestamp"],
    rows: mockEvents.slice(0, 10),
  },
  "SELECT COUNT(*) as total_records FROM users": {
    ok: true,
    message: "1 row returned",
    headers: ["total_records"],
    rows: [[1247]],
  },
  "SELECT COUNT(*) as total_users FROM users": {
    ok: true,
    message: "1 row returned",
    headers: ["total_users"],
    rows: [[1247]],
  },
  "SELECT * FROM orders WHERE order_date >= DATE('2024-01-01') ORDER BY order_date DESC LIMIT 20":
    {
      ok: true,
      message: "20 rows returned",
      headers: ["id", "user_id", "total_amount", "status", "order_date"],
      rows: [...mockOrders].reverse(),
    },
  "SELECT status, COUNT(*) as count FROM orders GROUP BY status": {
    ok: true,
    message: "4 rows returned",
    headers: ["status", "count"],
    rows: [
      ["completed", 1456],
      ["pending", 298],
      ["processing", 187],
      ["cancelled", 134],
    ],
  },
  "SELECT \n  CASE \n    WHEN age < 25 THEN 'Young (< 25)'\n    WHEN age < 40 THEN 'Adult (25-40)'\n    ELSE 'Senior (40+)'\n  END as age_group,\n  COUNT(*) as count\nFROM users \nGROUP BY age_group\nORDER BY count DESC":
    {
      ok: true,
      message: "3 rows returned",
      headers: ["age_group", "count"],
      rows: [
        ["Adult (25-40)", 789],
        ["Young (< 25)", 312],
        ["Senior (40+)", 146],
      ],
    },
  "SELECT \n  status,\n  COUNT(*) as order_count,\n  SUM(total_amount) as total_revenue,\n  AVG(total_amount) as avg_order_value\nFROM orders \nGROUP BY status\nORDER BY total_revenue DESC":
    {
      ok: true,
      message: "4 rows returned",
      headers: ["status", "order_count", "total_revenue", "avg_order_value"],
      rows: [
        ["completed", 1456, 287543.67, 197.49],
        ["pending", 298, 58932.45, 197.76],
        ["processing", 187, 36891.23, 197.28],
        ["cancelled", 134, 26234.89, 195.78],
      ],
    },
  "SELECT \n  p.name,\n  p.price,\n  p.category_id,\n  COUNT(*) as popularity_score\nFROM products p\nWHERE p.price > 50\nGROUP BY p.id, p.name, p.price, p.category_id\nORDER BY popularity_score DESC\nLIMIT 15":
    {
      ok: true,
      message: "15 rows returned",
      headers: ["name", "price", "category_id", "popularity_score"],
      rows: [
        ["Smart Fitness Watch", 299.99, 1, 89],
        ["Ergonomic Office Chair", 449.99, 3, 67],
        ["Air Purifier", 249.99, 6, 56],
        ["Standing Desk Converter", 299.99, 3, 54],
        ["4K Webcam", 199.99, 4, 45],
        ["Premium Coffee Maker", 189.99, 2, 43],
        ["Noise Cancelling Earbuds", 179.99, 1, 41],
        ["Mechanical Keyboard", 149.99, 4, 38],
        ["Wireless Bluetooth Headphones", 129.99, 1, 35],
        ["Smart Home Speaker", 99.99, 1, 32],
        ["Yoga Mat Premium", 89.99, 5, 29],
        ["Bluetooth Speaker Portable", 89.99, 1, 27],
        ["Laptop Stand Adjustable", 79.99, 4, 24],
        ["LED Desk Lamp", 79.99, 3, 22],
        ["Wireless Mouse", 69.99, 4, 19],
      ],
    },
  "SELECT \n  id, name, profile_vector,\n  VECTOR_DISTANCE(profile_vector, [0.1, 0.2, 0.3, 0.4]) as similarity\nFROM users\nWHERE VECTOR_DISTANCE(profile_vector, [0.1, 0.2, 0.3, 0.4]) < 0.5\nORDER BY similarity ASC\nLIMIT 10":
    {
      ok: true,
      message: "10 rows returned",
      headers: ["id", "name", "profile_vector", "similarity"],
      rows: [
        [1, "Alice Smith", "[0.12, 0.34, 0.56, 0.78]", 0.23],
        [3, "Charlie Brown", "[0.34, 0.56, 0.78, 0.12]", 0.31],
        [9, "Ivy Chen", "[0.11, 0.33, 0.55, 0.77]", 0.35],
        [17, "Quinn Thompson", "[0.19, 0.37, 0.51, 0.73]", 0.38],
        [2, "Bob Johnson", "[0.23, 0.45, 0.67, 0.89]", 0.41],
        [11, "Kate Moore", "[0.33, 0.55, 0.77, 0.11]", 0.43],
        [5, "Eve Adams", "[0.56, 0.78, 0.12, 0.34]", 0.44],
        [19, "Sam Harris", "[0.37, 0.55, 0.73, 0.19]", 0.46],
        [4, "Diana Prince", "[0.45, 0.67, 0.89, 0.23]", 0.47],
        [13, "Mia Anderson", "[0.55, 0.77, 0.11, 0.33]", 0.49],
      ],
    },
  "SELECT \n  name, price,\n  VECTOR_COSINE_SIMILARITY(embedding, [0.1, 0.2, ...]) as relevance\nFROM products\nWHERE VECTOR_COSINE_SIMILARITY(embedding, [0.1, 0.2, ...]) > 0.8\nORDER BY relevance DESC":
    {
      ok: true,
      message: "8 rows returned",
      headers: ["name", "price", "relevance"],
      rows: [
        ["Wireless Bluetooth Headphones", 129.99, 0.94],
        ["Smart Fitness Watch", 299.99, 0.91],
        ["Noise Cancelling Earbuds", 179.99, 0.89],
        ["Bluetooth Speaker Portable", 89.99, 0.87],
        ["Smart Home Speaker", 99.99, 0.85],
        ["Wireless Charging Pad", 39.99, 0.83],
        ["Portable Charger 20000mAh", 49.99, 0.82],
        ["4K Webcam", 199.99, 0.81],
      ],
    },
  "SELECT * FROM products WHERE price > 100 LIMIT 3": {
    ok: true,
    message: "3 rows returned",
    headers: ["id", "name", "price", "category_id", "embedding"],
    rows: mockProducts.filter((p) => p[2] > 100).slice(0, 3),
  },
  "SHOW TABLES": {
    ok: true,
    message: "4 rows returned",
    headers: ["table_name"],
    rows: [["users"], ["orders"], ["products"], ["analytics_events"]],
  },
  "SELECT * FROM users WHERE age > 25 ORDER BY created_at DESC": {
    ok: true,
    message: "15 rows returned",
    headers: ["id", "name", "email", "age", "created_at", "profile_vector"],
    rows: mockUsers.filter((u) => u[3] > 25).reverse(),
  },
  "SELECT o.*, u.name FROM orders o JOIN users u ON o.user_id = u.id WHERE o.status = 'pending'":
    {
      ok: true,
      message: "5 rows returned",
      headers: [
        "id",
        "user_id",
        "total_amount",
        "status",
        "order_date",
        "name",
      ],
      rows: [
        [2, 2, 149.5, "pending", "2024-01-21 10:15:30", "Bob Johnson"],
        [7, 6, 32.99, "pending", "2024-01-23 15:10:45", "Frank Miller"],
        [10, 9, 299.99, "pending", "2024-01-25 10:55:50", "Ivy Chen"],
        [15, 13, 159.5, "pending", "2024-01-27 13:00:45", "Mia Anderson"],
        [19, 16, 89.5, "pending", "2024-01-29 14:30:25", "Paul Rodriguez"],
      ],
    },
  "SELECT category_id, AVG(price) as avg_price FROM products GROUP BY category_id":
    {
      ok: true,
      message: "6 rows returned",
      headers: ["category_id", "avg_price"],
      rows: [
        [1, 143.32],
        [2, 124.99],
        [3, 276.66],
        [4, 89.98],
        [5, 62.49],
        [6, 249.99],
      ],
    },
};

export const useStore = create<State>()(
  persist(
    (set, get) => ({
      sql: "SELECT * FROM users LIMIT 5;",
      executing: false,
      history: [
        {
          ts: Date.now() - 300000,
          sql: "SELECT COUNT(*) as total_users FROM users;",
          result: mockResults["SELECT COUNT(*) as total_users FROM users"],
        },
        {
          ts: Date.now() - 600000,
          sql: "SELECT status, COUNT(*) as count FROM orders GROUP BY status;",
          result:
            mockResults[
              "SELECT status, COUNT(*) as count FROM orders GROUP BY status"
            ],
        },
      ],
      dpEpsilon: 1.0,
      txAsOf: undefined,
      darkMode: false,
      mockMode: true,
      tables: mockTables,
      favorites: [
        "SELECT * FROM users WHERE age > 25 ORDER BY created_at DESC;",
        "SELECT o.*, u.name FROM orders o JOIN users u ON o.user_id = u.id WHERE o.status = 'pending';",
        "SELECT category_id, AVG(price) as avg_price FROM products GROUP BY category_id;",
      ],
      setSql: (s) => set({ sql: s }),
      run: async (sql?: string) => {
        const state = get();
        const querySQL = (sql ?? state.sql).trim();
        const payload = {
          sql:
            querySQL +
            (state.dpEpsilon !== 1.0
              ? `\nSET DP_EPSILON = ${state.dpEpsilon};`
              : ""),
        };

        set({ executing: true });

        // Simulate network delay for better UX
        await new Promise((resolve) =>
          setTimeout(resolve, state.mockMode ? 500 : 100),
        );

        try {
          let result: QueryResult;

          if (state.mockMode) {
            // Use mock data
            const cleanSQL = querySQL.replace(/;$/, "");
            result = mockResults[cleanSQL] || {
              ok: true,
              message: "Query executed successfully (mock)",
              headers: ["result"],
              rows: [["Mock result for: " + cleanSQL]],
            };
          } else {
            // Use real API
            const r = await fetch("http://127.0.0.1:7070/query", {
              method: "POST",
              headers: { "Content-Type": "application/json" },
              body: JSON.stringify(payload),
            });
            result = (await r.json()) as QueryResult;
          }

          set((s) => ({
            result,
            history: [
              { ts: Date.now(), sql: querySQL, result },
              ...s.history,
            ].slice(0, 100),
          }));
        } catch (e: any) {
          const errorResult = {
            ok: false,
            message: e?.message ?? "request failed",
            headers: [],
            rows: [],
          };
          set((s) => ({
            result: errorResult,
            history: [
              { ts: Date.now(), sql: querySQL, result: errorResult },
              ...s.history,
            ],
          }));
        } finally {
          set({ executing: false });
        }
      },
      setDp: (e) => set({ dpEpsilon: e }),
      setTx: (tx) => set({ txAsOf: tx }),
      toggleDarkMode: () => set((s) => ({ darkMode: !s.darkMode })),
      toggleMockMode: () => set((s) => ({ mockMode: !s.mockMode })),
      addToFavorites: (sql) =>
        set((s) => ({
          favorites: [...s.favorites.filter((f) => f !== sql), sql].slice(-10),
        })),
      removeFromFavorites: (sql) =>
        set((s) => ({
          favorites: s.favorites.filter((f) => f !== sql),
        })),
    }),
    {
      name: "latticedb-storage",
      partialize: (state) => ({
        darkMode: state.darkMode,
        dpEpsilon: state.dpEpsilon,
        favorites: state.favorites,
        mockMode: state.mockMode,
        txAsOf: state.txAsOf,
      }),
    },
  ),
);
