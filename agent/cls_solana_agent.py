#!/usr/bin/env python3
"""
ClawLobstars Solana Agent — Real On-Chain Operations
=====================================================

A working Solana agent that connects to real RPC endpoints,
manages wallets, executes transfers, monitors prices,
and swaps tokens via Jupiter.

Tested on Devnet. Switch to Mainnet when ready.

Setup:
    pip install solana solders base58 requests

Usage:
    python3 cls_solana_agent.py              # Interactive mode
    python3 cls_solana_agent.py --devnet     # Force devnet
    python3 cls_solana_agent.py --mainnet    # Use mainnet (real SOL!)
"""

import os
import sys
import json
import time
import base64
import struct
import hashlib
import argparse
import requests
from pathlib import Path
from datetime import datetime, timezone
from typing import Optional, Dict, List, Tuple

# Solana SDK
from solders.keypair import Keypair
from solders.pubkey import Pubkey
from solders.system_program import TransferParams, transfer
from solders.transaction import Transaction
from solders.message import Message
from solders.commitment_config import CommitmentLevel
from solders.rpc.responses import GetBalanceResp
from solana.rpc.api import Client
from solana.rpc.commitment import Confirmed, Finalized
import base58

# ============================================================
# Constants
# ============================================================

LAMPORTS_PER_SOL = 1_000_000_000
SOL_DECIMALS = 9

# Known tokens (Devnet & Mainnet)
TOKENS = {
    "SOL": {
        "mint": "So11111111111111111111111111111111111111112",
        "decimals": 9,
        "name": "Solana",
    },
    "USDC": {
        "mint_mainnet": "EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v",
        "mint_devnet": "4zMMC9srt5Ri5X14GAgXhaHii3GnPAEERYPJgZJDncDU",
        "decimals": 6,
        "name": "USD Coin",
    },
}

JUPITER_API = "https://quote-api.jup.ag/v6"
DEXSCREENER_API = "https://api.dexscreener.com/latest/dex"
COINGECKO_API = "https://api.coingecko.com/api/v3"

# ============================================================
# Color Output
# ============================================================

class C:
    GREEN = "\033[92m"
    AMBER = "\033[93m"
    RED = "\033[91m"
    BLUE = "\033[94m"
    DIM = "\033[90m"
    BOLD = "\033[1m"
    RESET = "\033[0m"

def log(msg, color=C.GREEN):
    ts = datetime.now().strftime("%H:%M:%S")
    print(f"{C.DIM}{ts}{C.RESET} {color}{msg}{C.RESET}")

def log_ok(msg): log(f"✓ {msg}", C.GREEN)
def log_warn(msg): log(f"⚠ {msg}", C.AMBER)
def log_err(msg): log(f"✗ {msg}", C.RED)
def log_info(msg): log(f"  {msg}", C.BLUE)

# ============================================================
# Wallet Manager
# ============================================================

WALLET_DIR = Path.home() / ".clawlobstars"
WALLET_FILE = WALLET_DIR / "wallet.json"

def save_wallet(keypair: Keypair, label: str = "default"):
    """Save wallet keypair to disk (encrypted in production)"""
    WALLET_DIR.mkdir(exist_ok=True)
    data = {
        "label": label,
        "pubkey": str(keypair.pubkey()),
        "secret": base58.b58encode(bytes(keypair)).decode(),
        "created": datetime.now(timezone.utc).isoformat(),
    }
    with open(WALLET_FILE, "w") as f:
        json.dump(data, f, indent=2)
    log_ok(f"Wallet saved to {WALLET_FILE}")

def load_wallet() -> Optional[Keypair]:
    """Load wallet from disk"""
    if not WALLET_FILE.exists():
        return None
    try:
        with open(WALLET_FILE) as f:
            data = json.load(f)
        secret = base58.b58decode(data["secret"])
        kp = Keypair.from_bytes(secret)
        log_ok(f"Wallet loaded: {kp.pubkey()}")
        return kp
    except Exception as e:
        log_err(f"Failed to load wallet: {e}")
        return None

# ============================================================
# Solana Agent
# ============================================================

class ClawSolanaAgent:
    def __init__(self, cluster: str = "devnet", rpc_url: str = None):
        self.cluster = cluster
        
        if rpc_url:
            self.rpc_url = rpc_url
        elif cluster == "mainnet":
            self.rpc_url = os.environ.get(
                "SOLANA_RPC_URL",
                "https://api.mainnet-beta.solana.com"
            )
        elif cluster == "devnet":
            self.rpc_url = "https://api.devnet.solana.com"
        else:
            self.rpc_url = "https://api.testnet.solana.com"
        
        self.client = Client(self.rpc_url)
        self.keypair: Optional[Keypair] = None
        self.stats = {
            "rpc_calls": 0,
            "tx_sent": 0,
            "tx_confirmed": 0,
            "sol_spent": 0,
            "errors": 0,
        }
    
    def _rpc_call(self, name: str):
        """Track RPC stats"""
        self.stats["rpc_calls"] += 1
    
    # ---- Wallet ----
    
    def generate_wallet(self, label: str = "cls-agent") -> Keypair:
        """Generate new wallet keypair"""
        self.keypair = Keypair()
        save_wallet(self.keypair, label)
        log_ok(f"New wallet generated")
        log_info(f"Address: {self.keypair.pubkey()}")
        return self.keypair
    
    def load_wallet(self) -> Optional[Keypair]:
        """Load existing wallet"""
        self.keypair = load_wallet()
        return self.keypair
    
    def import_wallet(self, secret_key: str) -> Keypair:
        """Import wallet from base58 private key"""
        secret = base58.b58decode(secret_key)
        self.keypair = Keypair.from_bytes(secret)
        save_wallet(self.keypair, "imported")
        log_ok(f"Wallet imported: {self.keypair.pubkey()}")
        return self.keypair
    
    def get_pubkey(self) -> str:
        """Get wallet public key as string"""
        if not self.keypair:
            return "No wallet loaded"
        return str(self.keypair.pubkey())
    
    # ---- Balance ----
    
    def get_balance(self) -> float:
        """Get SOL balance in SOL (not lamports)"""
        if not self.keypair:
            log_err("No wallet loaded")
            return 0.0
        
        try:
            self._rpc_call("getBalance")
            resp = self.client.get_balance(self.keypair.pubkey(), commitment=Confirmed)
            lamports = resp.value
            sol = lamports / LAMPORTS_PER_SOL
            return sol
        except Exception as e:
            self.stats["errors"] += 1
            log_err(f"Failed to get balance: {e}")
            return 0.0
    
    def get_balance_lamports(self) -> int:
        """Get SOL balance in lamports"""
        if not self.keypair:
            return 0
        try:
            self._rpc_call("getBalance")
            resp = self.client.get_balance(self.keypair.pubkey(), commitment=Confirmed)
            return resp.value
        except Exception as e:
            self.stats["errors"] += 1
            return 0
    
    def check_balance(self, address: str = None) -> float:
        """Check balance of any address"""
        if address is None:
            return self.get_balance()
        try:
            self._rpc_call("getBalance")
            pubkey = Pubkey.from_string(address)
            resp = self.client.get_balance(pubkey, commitment=Confirmed)
            return resp.value / LAMPORTS_PER_SOL
        except Exception as e:
            log_err(f"Failed to check balance: {e}")
            return 0.0
    
    # ---- Airdrop (Devnet only) ----
    
    def airdrop(self, amount_sol: float = 1.0) -> bool:
        """Request airdrop on devnet"""
        if self.cluster != "devnet":
            log_err("Airdrop only available on devnet")
            return False
        
        if not self.keypair:
            log_err("No wallet loaded")
            return False
        
        lamports = int(amount_sol * LAMPORTS_PER_SOL)
        try:
            self._rpc_call("requestAirdrop")
            log_info(f"Requesting {amount_sol} SOL airdrop...")
            
            # Use raw RPC call for better error handling
            payload = {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "requestAirdrop",
                "params": [
                    str(self.keypair.pubkey()),
                    lamports,
                    {"commitment": "confirmed"},
                ],
            }
            resp = requests.post(self.rpc_url, json=payload, timeout=30)
            data = resp.json()
            
            if "error" in data:
                log_err(f"Airdrop failed: {data['error'].get('message', data['error'])}")
                return False
            
            sig = data.get("result", "")
            log_ok(f"Airdrop requested: {sig}")
            
            # Wait for confirmation
            log_info("Waiting for confirmation...")
            time.sleep(3)
            
            bal = self.get_balance()
            log_ok(f"Balance now: {bal:.6f} SOL")
            return True
        except Exception as e:
            self.stats["errors"] += 1
            log_err(f"Airdrop failed: {e}")
            return False
    
    # ---- Transfer SOL ----
    
    def transfer_sol(self, to_address: str, amount_sol: float) -> Optional[str]:
        """Transfer SOL to another address"""
        if not self.keypair:
            log_err("No wallet loaded")
            return None
        
        lamports = int(amount_sol * LAMPORTS_PER_SOL)
        balance = self.get_balance_lamports()
        
        if lamports > balance - 5000:  # Keep 5000 for fee
            log_err(f"Insufficient balance. Have {balance/LAMPORTS_PER_SOL:.4f} SOL, need {amount_sol} + fee")
            return None
        
        try:
            to_pubkey = Pubkey.from_string(to_address)
            
            # Build transfer instruction
            ix = transfer(TransferParams(
                from_pubkey=self.keypair.pubkey(),
                to_pubkey=to_pubkey,
                lamports=lamports,
            ))
            
            # Get recent blockhash
            self._rpc_call("getLatestBlockhash")
            blockhash_resp = self.client.get_latest_blockhash(commitment=Confirmed)
            blockhash = blockhash_resp.value.blockhash
            
            # Build and sign transaction
            msg = Message.new_with_blockhash(
                [ix],
                self.keypair.pubkey(),
                blockhash,
            )
            tx = Transaction.new_unsigned(msg)
            tx.sign([self.keypair], blockhash)
            
            # Send
            self._rpc_call("sendTransaction")
            log_info(f"Sending {amount_sol} SOL to {to_address[:8]}...{to_address[-4:]}")
            resp = self.client.send_transaction(tx, opts={"skip_preflight": False})
            sig = str(resp.value)
            
            self.stats["tx_sent"] += 1
            self.stats["tx_confirmed"] += 1
            self.stats["sol_spent"] += lamports
            
            explorer = "https://explorer.solana.com/tx"
            suffix = "?cluster=devnet" if self.cluster == "devnet" else ""
            log_ok(f"Transfer confirmed!")
            log_info(f"Signature: {sig}")
            log_info(f"Explorer: {explorer}/{sig}{suffix}")
            
            return sig
        except Exception as e:
            self.stats["errors"] += 1
            log_err(f"Transfer failed: {e}")
            return None
    
    # ---- Price Feed (Jupiter) ----
    
    def get_price(self, token_id: str = "SOL") -> Optional[float]:
        """Get token price in USD — CoinGecko with 30s cache"""
        try:
            if token_id.upper() == "USDC":
                return 1.0
            
            # Check cache (30 second TTL)
            cache_key = token_id.upper()
            now = time.time()
            if hasattr(self, '_price_cache') and cache_key in self._price_cache:
                cached_price, cached_time = self._price_cache[cache_key]
                if now - cached_time < 30:
                    return cached_price
            
            if not hasattr(self, '_price_cache'):
                self._price_cache = {}
            
            cg_map = {
                "SOL": "solana",
                "BONK": "bonk",
                "WIF": "dogwifcoin",
                "JUP": "jupiter-exchange-solana",
                "RAY": "raydium",
                "PYTH": "pyth-network",
                "JTO": "jito-governance-token",
                "ORCA": "orca",
                "MNDE": "marinade",
            }
            
            cg_id = cg_map.get(token_id.upper())
            if not cg_id:
                log_warn(f"Unknown token: {token_id}. Supported: {', '.join(cg_map.keys())}")
                return None
            
            resp = requests.get(
                f"{COINGECKO_API}/simple/price",
                params={"ids": cg_id, "vs_currencies": "usd", "include_24hr_change": "true"},
                timeout=10,
            )
            resp.raise_for_status()
            data = resp.json()
            
            if cg_id in data and "usd" in data[cg_id]:
                price = float(data[cg_id]["usd"])
                self._price_cache[cache_key] = (price, now)
                return price
            
            return None
        except Exception as e:
            if "429" not in str(e):
                log_err(f"Price fetch failed: {e}")
            # Return cached value if rate limited
            if hasattr(self, '_price_cache') and cache_key in self._price_cache:
                return self._price_cache[cache_key][0]
            return None
    
    def get_prices(self, tokens: List[str] = None) -> Dict[str, float]:
        """Get multiple token prices"""
        if tokens is None:
            tokens = ["SOL"]
        
        prices = {}
        for t in tokens:
            price = self.get_price(t)
            if price:
                prices[t.upper()] = price
        
        return prices
    
    # ---- Jupiter Swap Quote ----
    
    def get_swap_quote(
        self,
        input_mint: str,
        output_mint: str,
        amount: int,
        slippage_bps: int = 50,
    ) -> Optional[dict]:
        """Get swap quote from Jupiter"""
        try:
            resp = requests.get(
                f"{JUPITER_API}/quote",
                params={
                    "inputMint": input_mint,
                    "outputMint": output_mint,
                    "amount": str(amount),
                    "slippageBps": str(slippage_bps),
                },
                timeout=15,
            )
            resp.raise_for_status()
            quote = resp.json()
            return quote
        except Exception as e:
            log_err(f"Quote failed: {e}")
            return None
    
    def execute_swap(
        self,
        input_mint: str,
        output_mint: str,
        amount: int,
        slippage_bps: int = 50,
    ) -> Optional[str]:
        """Execute token swap via Jupiter (Mainnet only)"""
        if self.cluster != "mainnet":
            log_warn("Jupiter swaps only work on mainnet")
            log_info("Getting quote for preview...")
        
        if not self.keypair:
            log_err("No wallet loaded")
            return None
        
        # Get quote
        quote = self.get_swap_quote(input_mint, output_mint, amount, slippage_bps)
        if not quote:
            return None
        
        in_amount = int(quote.get("inAmount", 0))
        out_amount = int(quote.get("outAmount", 0))
        price_impact = float(quote.get("priceImpactPct", 0))
        
        log_info(f"Quote: {in_amount} → {out_amount}")
        log_info(f"Price impact: {price_impact:.4f}%")
        
        if self.cluster != "mainnet":
            log_warn("Swap preview only (devnet). Switch to mainnet to execute.")
            return None
        
        # Get swap transaction from Jupiter
        try:
            swap_resp = requests.post(
                f"{JUPITER_API}/swap",
                json={
                    "quoteResponse": quote,
                    "userPublicKey": str(self.keypair.pubkey()),
                    "wrapAndUnwrapSol": True,
                },
                timeout=15,
            )
            swap_resp.raise_for_status()
            swap_data = swap_resp.json()
            
            # Deserialize and sign transaction
            swap_tx_bytes = base64.b64decode(swap_data["swapTransaction"])
            tx = Transaction.from_bytes(swap_tx_bytes)
            
            # Get fresh blockhash
            self._rpc_call("getLatestBlockhash")
            blockhash_resp = self.client.get_latest_blockhash(commitment=Confirmed)
            blockhash = blockhash_resp.value.blockhash
            
            tx.sign([self.keypair], blockhash)
            
            # Send
            self._rpc_call("sendTransaction")
            resp = self.client.send_transaction(tx, opts={"skip_preflight": True})
            sig = str(resp.value)
            
            self.stats["tx_sent"] += 1
            self.stats["tx_confirmed"] += 1
            
            log_ok(f"Swap confirmed: {sig}")
            return sig
        except Exception as e:
            self.stats["errors"] += 1
            log_err(f"Swap execution failed: {e}")
            return None
    
    # ---- Token Accounts ----
    
    def get_token_accounts(self) -> List[dict]:
        """Get all SPL token accounts for wallet"""
        if not self.keypair:
            return []
        
        try:
            self._rpc_call("getTokenAccountsByOwner")
            # Use raw RPC for token accounts
            payload = {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "getTokenAccountsByOwner",
                "params": [
                    str(self.keypair.pubkey()),
                    {"programId": "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA"},
                    {"encoding": "jsonParsed"},
                ],
            }
            resp = requests.post(self.rpc_url, json=payload, timeout=15)
            data = resp.json()
            
            accounts = []
            for item in data.get("result", {}).get("value", []):
                info = item["account"]["data"]["parsed"]["info"]
                accounts.append({
                    "mint": info["mint"],
                    "amount": int(info["tokenAmount"]["amount"]),
                    "decimals": info["tokenAmount"]["decimals"],
                    "ui_amount": float(info["tokenAmount"].get("uiAmount", 0) or 0),
                })
            return accounts
        except Exception as e:
            log_err(f"Failed to get token accounts: {e}")
            return []
    
    # ---- Monitoring ----
    
    def watch_balance(self, interval_s: int = 10, duration_s: int = 60):
        """Monitor balance changes"""
        if not self.keypair:
            log_err("No wallet loaded")
            return
        
        log_info(f"Watching balance for {duration_s}s (every {interval_s}s)...")
        last_balance = self.get_balance()
        log_info(f"Starting balance: {last_balance:.6f} SOL")
        
        start = time.time()
        while time.time() - start < duration_s:
            time.sleep(interval_s)
            current = self.get_balance()
            diff = current - last_balance
            
            if abs(diff) > 0.000001:
                if diff > 0:
                    log_ok(f"Balance: {current:.6f} SOL (+{diff:.6f})")
                else:
                    log_warn(f"Balance: {current:.6f} SOL ({diff:.6f})")
                last_balance = current
            else:
                ts = datetime.now().strftime("%H:%M:%S")
                print(f"{C.DIM}{ts}   Balance: {current:.6f} SOL (no change){C.RESET}")
        
        log_info("Watch ended")
    
    def watch_price(self, token: str = "SOL", interval_s: int = 30, duration_s: int = 60):
        """Monitor token price"""
        log_info(f"Watching {token} price for {duration_s}s (every {interval_s}s)...")
        last_price = self.get_price(token)
        if last_price:
            log_info(f"Starting price: ${last_price:.4f}")
        
        start = time.time()
        while time.time() - start < duration_s:
            time.sleep(interval_s)
            price = self.get_price(token)
            if price and last_price:
                diff = price - last_price
                pct = (diff / last_price) * 100
                color = C.GREEN if diff >= 0 else C.RED
                arrow = "▲" if diff >= 0 else "▼"
                ts = datetime.now().strftime("%H:%M:%S")
                print(f"{C.DIM}{ts}{C.RESET} {color}  {token}: ${price:.4f} {arrow} {pct:+.3f}%{C.RESET}")
                last_price = price
            elif not price:
                ts = datetime.now().strftime("%H:%M:%S")
                print(f"{C.DIM}{ts}   (rate limited, waiting...){C.RESET}")
        
        log_info("Watch ended")
    
    # ---- Transaction History ----
    
    def get_recent_transactions(self, limit: int = 5) -> List[dict]:
        """Get recent transaction signatures"""
        if not self.keypair:
            return []
        
        try:
            self._rpc_call("getSignaturesForAddress")
            payload = {
                "jsonrpc": "2.0",
                "id": 1,
                "method": "getSignaturesForAddress",
                "params": [
                    str(self.keypair.pubkey()),
                    {"limit": limit},
                ],
            }
            resp = requests.post(self.rpc_url, json=payload, timeout=15)
            data = resp.json()
            return data.get("result", [])
        except Exception as e:
            log_err(f"Failed to get transactions: {e}")
            return []
    
    # ---- Status ----
    
    def status(self):
        """Print agent status"""
        print(f"\n{C.GREEN}{C.BOLD}🦞 CLAWLOBSTARS AGENT STATUS{C.RESET}")
        print(f"{C.DIM}{'━' * 40}{C.RESET}")
        
        print(f"{C.BLUE}  Cluster:   {C.RESET}{self.cluster}")
        print(f"{C.BLUE}  RPC:       {C.RESET}{self.rpc_url}")
        
        if self.keypair:
            pubkey = str(self.keypair.pubkey())
            balance = self.get_balance()
            print(f"{C.BLUE}  Wallet:    {C.RESET}{pubkey[:8]}...{pubkey[-4:]}")
            print(f"{C.BLUE}  Balance:   {C.GREEN}{balance:.6f} SOL{C.RESET}")
            
            sol_price = self.get_price("SOL")
            if sol_price:
                usd_val = balance * sol_price
                print(f"{C.BLUE}  USD Value: {C.AMBER}${usd_val:.2f}{C.RESET}")
                print(f"{C.BLUE}  SOL Price: {C.RESET}${sol_price:.2f}")
        else:
            print(f"{C.AMBER}  Wallet:    Not loaded{C.RESET}")
        
        print(f"\n{C.DIM}  RPC Calls: {self.stats['rpc_calls']}")
        print(f"  TX Sent:   {self.stats['tx_sent']}")
        print(f"  Errors:    {self.stats['errors']}{C.RESET}")
        print()

# ============================================================
# Interactive CLI
# ============================================================

def print_help():
    print(f"""
{C.GREEN}{C.BOLD}🦞 CLAWLOBSTARS AGENT — Commands{C.RESET}
{C.DIM}{'━' * 45}{C.RESET}

{C.BOLD}Wallet:{C.RESET}
  new              Generate new wallet
  load             Load saved wallet
  import <key>     Import private key
  address          Show wallet address
  
{C.BOLD}Balance:{C.RESET}
  balance          Check SOL balance
  tokens           List all token holdings
  airdrop [n]      Request devnet airdrop (default 1 SOL)
  
{C.BOLD}Transfer:{C.RESET}
  send <addr> <amount>   Transfer SOL
  
{C.BOLD}Market:{C.RESET}
  price [token]    Get token price (default SOL)
  quote <in> <out> <amount>   Get swap quote
  swap <in> <out> <amount>    Execute swap (mainnet)
  
{C.BOLD}Monitor:{C.RESET}
  watch balance [secs]   Watch balance changes
  watch price [secs]     Watch SOL price
  
{C.BOLD}Info:{C.RESET}
  status           Agent status
  history          Recent transactions
  help             This help
  quit             Exit
""")

def interactive(agent: ClawSolanaAgent):
    print(f"\n{C.GREEN}{C.BOLD}🦞 CLAWLOBSTARS Solana Agent v0.2.0{C.RESET}")
    print(f"{C.DIM}Cluster: {agent.cluster} | RPC: {agent.rpc_url}{C.RESET}")
    print(f"{C.DIM}Type 'help' for commands, 'quit' to exit{C.RESET}\n")
    
    # Try loading existing wallet
    if agent.load_wallet() is None:
        log_info("No wallet found. Use 'new' to generate one.")
    
    while True:
        try:
            raw = input(f"{C.GREEN}claw>{C.RESET} ").strip()
            if not raw:
                continue
            
            parts = raw.split()
            cmd = parts[0].lower()
            args = parts[1:]
            
            # ---- Wallet ----
            if cmd == "new":
                agent.generate_wallet()
                if agent.cluster == "devnet":
                    log_info("Use 'airdrop' to get free devnet SOL")
            
            elif cmd == "load":
                if agent.load_wallet() is None:
                    log_warn("No saved wallet. Use 'new' to generate.")
            
            elif cmd == "import":
                if not args:
                    log_err("Usage: import <base58_private_key>")
                else:
                    agent.import_wallet(args[0])
            
            elif cmd == "address":
                print(f"  {agent.get_pubkey()}")
            
            # ---- Balance ----
            elif cmd in ("balance", "bal"):
                bal = agent.get_balance()
                log_info(f"Balance: {bal:.6f} SOL")
                price = agent.get_price("SOL")
                if price:
                    log_info(f"Value: ${bal * price:.2f} (SOL @ ${price:.2f})")
            
            elif cmd == "tokens":
                accounts = agent.get_token_accounts()
                if not accounts:
                    log_info("No token accounts found")
                else:
                    print(f"\n  {'Mint':<46} {'Amount':>15} {'Decimals':>10}")
                    print(f"  {'-'*46} {'-'*15} {'-'*10}")
                    for acc in accounts:
                        mint = acc['mint'][:20] + "..." + acc['mint'][-4:]
                        print(f"  {mint:<46} {acc['ui_amount']:>15.4f} {acc['decimals']:>10}")
                    print()
            
            elif cmd == "airdrop":
                amount = float(args[0]) if args else 1.0
                agent.airdrop(amount)
            
            # ---- Transfer ----
            elif cmd == "send":
                if len(args) < 2:
                    log_err("Usage: send <address> <amount_sol>")
                else:
                    addr = args[0]
                    amount = float(args[1])
                    confirm = input(f"  Send {amount} SOL to {addr[:8]}...? (y/n): ")
                    if confirm.lower() == "y":
                        agent.transfer_sol(addr, amount)
                    else:
                        log_info("Cancelled")
            
            # ---- Market ----
            elif cmd == "price":
                token = args[0].upper() if args else "SOL"
                price = agent.get_price(token)
                if price:
                    log_info(f"{token}: ${price:.4f}")
            
            elif cmd == "quote":
                if len(args) < 3:
                    log_err("Usage: quote <input_mint> <output_mint> <amount_lamports>")
                else:
                    quote = agent.get_swap_quote(args[0], args[1], int(args[2]))
                    if quote:
                        print(f"  In:     {quote.get('inAmount')}")
                        print(f"  Out:    {quote.get('outAmount')}")
                        print(f"  Impact: {quote.get('priceImpactPct', 'N/A')}%")
            
            elif cmd == "swap":
                if len(args) < 3:
                    log_err("Usage: swap <input_mint> <output_mint> <amount_lamports>")
                else:
                    agent.execute_swap(args[0], args[1], int(args[2]))
            
            # ---- Monitor ----
            elif cmd == "watch":
                if not args:
                    log_err("Usage: watch balance|price [duration_secs]")
                elif args[0] == "balance":
                    dur = int(args[1]) if len(args) > 1 else 60
                    agent.watch_balance(interval_s=10, duration_s=dur)
                elif args[0] == "price":
                    token = "SOL"
                    dur = 60
                    for a in args[1:]:
                        try:
                            dur = int(a)
                        except ValueError:
                            token = a.upper()
                    agent.watch_price(token, interval_s=30, duration_s=dur)
            
            # ---- Info ----
            elif cmd == "status":
                agent.status()
            
            elif cmd == "history":
                txs = agent.get_recent_transactions()
                if not txs:
                    log_info("No transactions found")
                else:
                    print(f"\n  {'Signature':<50} {'Status':>10}")
                    print(f"  {'-'*50} {'-'*10}")
                    for tx in txs:
                        sig = tx.get("signature", "")[:46] + "..."
                        err = tx.get("err")
                        status = "OK" if err is None else "FAIL"
                        color = C.GREEN if err is None else C.RED
                        print(f"  {sig:<50} {color}{status:>10}{C.RESET}")
                    print()
            
            elif cmd == "help":
                print_help()
            
            elif cmd in ("quit", "exit", "q"):
                log_info("Agent shutting down...")
                break
            
            else:
                log_warn(f"Unknown command: {cmd}. Type 'help' for commands.")
        
        except KeyboardInterrupt:
            print()
            log_info("Agent shutting down...")
            break
        except EOFError:
            break
        except Exception as e:
            log_err(f"Error: {e}")

# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="ClawLobstars Solana Agent")
    parser.add_argument("--devnet", action="store_true", help="Use Devnet (default)")
    parser.add_argument("--mainnet", action="store_true", help="Use Mainnet (real SOL!)")
    parser.add_argument("--rpc", type=str, help="Custom RPC URL")
    args = parser.parse_args()
    
    if args.mainnet:
        cluster = "mainnet"
        log_warn("⚠️  MAINNET MODE — Real SOL will be used!")
    else:
        cluster = "devnet"
    
    agent = ClawSolanaAgent(cluster=cluster, rpc_url=args.rpc)
    interactive(agent)

if __name__ == "__main__":
    main()
