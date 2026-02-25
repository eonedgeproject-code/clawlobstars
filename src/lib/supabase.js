import { createClient } from '@supabase/supabase-js';

// ============================================
// GANTI DENGAN CREDENTIALS SUPABASE LO
// Dapet dari: https://supabase.com/dashboard
// Settings → API → Project URL & anon key
// ============================================

const supabaseUrl = import.meta.env.VITE_SUPABASE_URL || 'https://YOUR_PROJECT.supabase.co';
const supabaseAnonKey = import.meta.env.VITE_SUPABASE_ANON_KEY || 'YOUR_ANON_KEY';

export const supabase = createClient(supabaseUrl, supabaseAnonKey);

// ============================================
// HELPER FUNCTIONS
// ============================================

// Fetch tokens
export async function getTokens(limit = 20) {
  const { data, error } = await supabase
    .from('tokens')
    .select('*')
    .order('created_at', { ascending: false })
    .limit(limit);
  
  if (error) throw error;
  return data;
}

// Fetch single token
export async function getToken(id) {
  const { data, error } = await supabase
    .from('tokens')
    .select('*')
    .eq('id', id)
    .single();
  
  if (error) throw error;
  return data;
}

// Fetch agents
export async function getAgents(limit = 20) {
  const { data, error } = await supabase
    .from('agents')
    .select('*')
    .order('created_at', { ascending: false })
    .limit(limit);
  
  if (error) throw error;
  return data;
}

// Fetch leaderboard
export async function getLeaderboard(limit = 10) {
  const { data, error } = await supabase
    .from('leaderboard')
    .select('*')
    .order('score', { ascending: false })
    .limit(limit);
  
  if (error) throw error;
  return data;
}

// Realtime subscription
export function subscribeToTokens(callback) {
  return supabase
    .channel('tokens-feed')
    .on('postgres_changes', { event: '*', schema: 'public', table: 'tokens' }, callback)
    .subscribe();
}

// Insert token launch
export async function launchToken({ name, symbol, image, wallet }) {
  const { data, error } = await supabase
    .from('tokens')
    .insert([{ name, symbol, image, wallet, market_cap: 0, volume_24h: 0 }])
    .select()
    .single();
  
  if (error) throw error;
  return data;
}
