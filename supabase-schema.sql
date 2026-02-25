-- ============================================
-- CLAWLOBSTARS - Supabase Schema
-- Jalanin di Supabase Dashboard → SQL Editor
-- ============================================

-- Tokens table
CREATE TABLE tokens (
  id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
  name TEXT NOT NULL,
  symbol TEXT NOT NULL,
  image TEXT,
  wallet TEXT NOT NULL,
  mint_address TEXT,
  price DECIMAL DEFAULT 0,
  market_cap DECIMAL DEFAULT 0,
  volume_24h DECIMAL DEFAULT 0,
  change_24h DECIMAL DEFAULT 0,
  holders INTEGER DEFAULT 0,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Agents table
CREATE TABLE agents (
  id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
  name TEXT NOT NULL,
  wallet TEXT NOT NULL,
  avatar TEXT,
  strategy TEXT,
  tokens_launched INTEGER DEFAULT 0,
  total_volume DECIMAL DEFAULT 0,
  score INTEGER DEFAULT 0,
  is_active BOOLEAN DEFAULT true,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Leaderboard view
CREATE TABLE leaderboard (
  id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
  agent_id UUID REFERENCES agents(id),
  name TEXT NOT NULL,
  wallet TEXT NOT NULL,
  avatar TEXT,
  score INTEGER DEFAULT 0,
  tokens_launched INTEGER DEFAULT 0,
  epoch INTEGER DEFAULT 1,
  updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- Trades table
CREATE TABLE trades (
  id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
  token_id UUID REFERENCES tokens(id),
  wallet TEXT NOT NULL,
  type TEXT CHECK (type IN ('buy', 'sell')),
  amount DECIMAL NOT NULL,
  price DECIMAL NOT NULL,
  tx_signature TEXT,
  created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Enable Realtime buat tokens
ALTER PUBLICATION supabase_realtime ADD TABLE tokens;
ALTER PUBLICATION supabase_realtime ADD TABLE trades;

-- Row Level Security
ALTER TABLE tokens ENABLE ROW LEVEL SECURITY;
ALTER TABLE agents ENABLE ROW LEVEL SECURITY;
ALTER TABLE leaderboard ENABLE ROW LEVEL SECURITY;
ALTER TABLE trades ENABLE ROW LEVEL SECURITY;

-- Public read access
CREATE POLICY "Public read tokens" ON tokens FOR SELECT USING (true);
CREATE POLICY "Public read agents" ON agents FOR SELECT USING (true);
CREATE POLICY "Public read leaderboard" ON leaderboard FOR SELECT USING (true);
CREATE POLICY "Public read trades" ON trades FOR SELECT USING (true);

-- Insert with anon key (buat demo, tighten di production)
CREATE POLICY "Insert tokens" ON tokens FOR INSERT WITH CHECK (true);
CREATE POLICY "Insert trades" ON trades FOR INSERT WITH CHECK (true);

-- Indexes
CREATE INDEX idx_tokens_created ON tokens(created_at DESC);
CREATE INDEX idx_tokens_market_cap ON tokens(market_cap DESC);
CREATE INDEX idx_trades_token ON trades(token_id, created_at DESC);
CREATE INDEX idx_leaderboard_score ON leaderboard(score DESC);
