#!/usr/bin/env python3
"""
ClawLobstars Telegram Bot — Agent Command & Control
====================================================

Control your ClawLobstars agents directly from Telegram.
Supports agent management, staking info, wallet checks,
and real-time monitoring.

Requirements:
    pip install python-telegram-bot==20.7

Setup:
    1. Create bot via @BotFather on Telegram
    2. Copy token to TELEGRAM_BOT_TOKEN env var
    3. Run: python3 cls_telegram_bot.py

Commands:
    /start          — Welcome + help
    /status         — All agents status
    /agents         — List deployed agents
    /agent <id>     — Agent detail
    /deploy <name>  — Deploy new agent
    /kill <id>      — Kill agent
    /pause <id>     — Pause agent
    /resume <id>    — Resume agent
    /stake          — Staking overview
    /wallet         — Wallet balance
    /revenue        — Revenue stats
    /governance     — Active proposals
    /vote <id> <y/n>— Vote on proposal
    /logs [n]       — Recent logs
    /health         — System health
    /help           — Command list
"""

import os
import json
import time
import random
import asyncio
import logging
from datetime import datetime, timezone
from dataclasses import dataclass, field
from typing import Dict, List, Optional

from telegram import Update, InlineKeyboardButton, InlineKeyboardMarkup
from telegram.ext import (
    Application,
    CommandHandler,
    CallbackQueryHandler,
    ContextTypes,
)

# ============================================================
# Configuration
# ============================================================

BOT_TOKEN = os.environ.get("TELEGRAM_BOT_TOKEN", "YOUR_TOKEN_HERE")
AUTHORIZED_USERS: List[int] = []  # Add your Telegram user IDs here. Empty = allow all.

logging.basicConfig(
    format="%(asctime)s [CLS-BOT] %(levelname)s: %(message)s",
    level=logging.INFO,
)
logger = logging.getLogger(__name__)

# ============================================================
# Data Models (Simulated — replace with real C bridge / RPC)
# ============================================================

@dataclass
class Agent:
    id: int
    name: str
    status: str = "running"      # running, idle, paused, stopped
    cpu: float = 0.0
    memory_kb: int = 0
    steps: int = 0
    uptime_s: int = 0
    model_type: str = "rule_based"
    inference_us: float = 0.34

@dataclass
class StakeInfo:
    tier: str = "Commander"
    amount: int = 125000
    apy: float = 17.0
    rewards_earned: int = 4271
    total_staked: int = 847200
    agent_slots: int = 20
    agents_used: int = 3

@dataclass
class WalletInfo:
    address: str = "7xKp...f3Qm"
    sol_balance: float = 4.127
    claw_balance: int = 125000
    bonk_balance: int = 14600000
    usdc_balance: float = 847.50

@dataclass
class Proposal:
    id: str
    title: str
    status: str           # active, passed, rejected
    votes_for: int
    votes_against: int
    days_left: int = 0

@dataclass
class SystemState:
    agents: Dict[int, Agent] = field(default_factory=dict)
    stake: StakeInfo = field(default_factory=StakeInfo)
    wallet: WalletInfo = field(default_factory=WalletInfo)
    proposals: List[Proposal] = field(default_factory=list)
    logs: List[str] = field(default_factory=list)
    revenue_epoch: int = 2847
    next_agent_id: int = 4

# Initialize simulated state
state = SystemState()
state.agents = {
    1: Agent(1, "tactical-01", "running", 12.4, 234, 14847, 170520, "rule_based", 0.34),
    2: Agent(2, "sniper-02", "running", 8.7, 189, 9241, 134200, "neural_net", 0.28),
    3: Agent(3, "recon-03", "idle", 2.1, 312, 6102, 98400, "decision_tree", 0.41),
}
state.proposals = [
    Proposal("CLS-008", "Increase Operative slots from 5 → 8", "active", 412000, 198000, 2),
    Proposal("CLS-009", "Add Raydium concentrated liquidity module", "active", 287000, 63000, 5),
    Proposal("CLS-007", "Reduce early unstake penalty to 10%", "passed", 546000, 54000),
]
state.logs = [
    "14:22:08 [AGENT] tactical-01 step #14847 — 0.34µs",
    "14:22:07 [SOLANA] swap: 0.5 SOL → 14.6M BONK",
    "14:21:52 [RESOURCE] recon-03 memory 78% — auto-prune",
    "14:21:44 [PERCEPTION] recon-03 price anomaly SOL/USDC +3.2%",
    "14:21:15 [TOKEN] epoch: 2847 CLAW distributed",
    "14:20:30 [GOVERNANCE] CLS-007 passed — quorum 14.2%",
]

# ============================================================
# Helpers
# ============================================================

STATUS_EMOJI = {
    "running": "🟢",
    "idle": "🟡",
    "paused": "⏸️",
    "stopped": "🔴",
}

def auth_check(user_id: int) -> bool:
    if not AUTHORIZED_USERS:
        return True
    return user_id in AUTHORIZED_USERS

def format_number(n: int) -> str:
    if n >= 1_000_000:
        return f"{n / 1_000_000:.1f}M"
    elif n >= 1_000:
        return f"{n / 1_000:.1f}K"
    return str(n)

def add_log(msg: str):
    now = datetime.now(timezone.utc).strftime("%H:%M:%S")
    state.logs.insert(0, f"{now} {msg}")
    if len(state.logs) > 100:
        state.logs = state.logs[:100]

# ============================================================
# Command Handlers
# ============================================================

async def cmd_start(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        await update.message.reply_text("⛔ Unauthorized.")
        return

    text = (
        "🦞 *CLAWLOBSTARS Command Bot*\n"
        "━━━━━━━━━━━━━━━━━━━━━\n\n"
        "Agent framework C&C interface.\n"
        "Control your autonomous agents from Telegram.\n\n"
        "*Quick Commands:*\n"
        "/status — System overview\n"
        "/agents — List agents\n"
        "/stake — Staking info\n"
        "/wallet — Wallet balance\n"
        "/help — All commands\n\n"
        f"_Framework v0.2.0 • {len(state.agents)} agents deployed_"
    )
    await update.message.reply_text(text, parse_mode="Markdown")


async def cmd_help(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    text = (
        "🦞 *Command Reference*\n"
        "━━━━━━━━━━━━━━━━━━━━━\n\n"
        "*Operations*\n"
        "`/status` — System overview\n"
        "`/agents` — List all agents\n"
        "`/agent <id>` — Agent details\n"
        "`/deploy <name>` — Deploy new agent\n"
        "`/kill <id>` — Terminate agent\n"
        "`/pause <id>` — Pause agent\n"
        "`/resume <id>` — Resume agent\n\n"
        "*Finance*\n"
        "`/stake` — Staking overview\n"
        "`/wallet` — Wallet balance\n"
        "`/revenue` — Revenue breakdown\n\n"
        "*Governance*\n"
        "`/governance` — Active proposals\n"
        "`/vote <id> <y/n>` — Cast vote\n\n"
        "*System*\n"
        "`/logs [n]` — Recent logs (default 10)\n"
        "`/health` — System health check\n"
    )
    await update.message.reply_text(text, parse_mode="Markdown")


async def cmd_status(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    running = sum(1 for a in state.agents.values() if a.status == "running")
    idle = sum(1 for a in state.agents.values() if a.status == "idle")
    total_cpu = sum(a.cpu for a in state.agents.values())
    total_mem = sum(a.memory_kb for a in state.agents.values())

    text = (
        "🦞 *SYSTEM STATUS*\n"
        "━━━━━━━━━━━━━━━━━━━━━\n\n"
        f"🟢 Agents Running: *{running}*\n"
        f"🟡 Agents Idle: *{idle}*\n"
        f"📊 Total CPU: *{total_cpu:.1f}%*\n"
        f"💾 Total Memory: *{total_mem} KB*\n"
        f"⚡ Inference: *9.6M ops/s*\n\n"
        f"💰 Staked: *{format_number(state.stake.total_staked)} CLAW*\n"
        f"📈 Epoch Revenue: *{format_number(state.revenue_epoch)} CLAW*\n"
        f"💳 SOL Balance: *{state.wallet.sol_balance:.3f} SOL*\n\n"
        f"_Last updated: {datetime.now(timezone.utc).strftime('%H:%M:%S')} UTC_"
    )

    keyboard = [
        [
            InlineKeyboardButton("📋 Agents", callback_data="cb_agents"),
            InlineKeyboardButton("💰 Stake", callback_data="cb_stake"),
        ],
        [
            InlineKeyboardButton("💳 Wallet", callback_data="cb_wallet"),
            InlineKeyboardButton("📜 Logs", callback_data="cb_logs"),
        ],
    ]
    await update.message.reply_text(
        text, parse_mode="Markdown", reply_markup=InlineKeyboardMarkup(keyboard)
    )


async def cmd_agents(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    lines = ["🦞 *DEPLOYED AGENTS*\n━━━━━━━━━━━━━━━━━━━━━\n"]
    for a in state.agents.values():
        emoji = STATUS_EMOJI.get(a.status, "⚪")
        lines.append(
            f"{emoji} *{a.name}* (#{a.id})\n"
            f"   Status: `{a.status}` | CPU: `{a.cpu}%` | Mem: `{a.memory_kb}KB`\n"
            f"   Steps: `{format_number(a.steps)}` | Latency: `{a.inference_us}µs`\n"
        )

    slots = state.stake.agent_slots
    used = len(state.agents)
    lines.append(f"\n_Slots: {used}/{slots} ({state.stake.tier} tier)_")
    await update.message.reply_text("\n".join(lines), parse_mode="Markdown")


async def cmd_agent_detail(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    if not context.args:
        await update.message.reply_text("Usage: `/agent <id>`", parse_mode="Markdown")
        return

    try:
        agent_id = int(context.args[0])
    except ValueError:
        await update.message.reply_text("⚠️ Invalid agent ID.")
        return

    a = state.agents.get(agent_id)
    if not a:
        await update.message.reply_text(f"⚠️ Agent #{agent_id} not found.")
        return

    emoji = STATUS_EMOJI.get(a.status, "⚪")
    uptime_h = a.uptime_s / 3600

    text = (
        f"🦞 *AGENT: {a.name}*\n"
        f"━━━━━━━━━━━━━━━━━━━━━\n\n"
        f"ID: `#{a.id}`\n"
        f"Status: {emoji} `{a.status}`\n"
        f"Model: `{a.model_type}`\n"
        f"CPU: `{a.cpu}%`\n"
        f"Memory: `{a.memory_kb} KB`\n"
        f"Inference: `{a.inference_us} µs`\n"
        f"Steps: `{format_number(a.steps)}`\n"
        f"Uptime: `{uptime_h:.1f}h`\n"
    )

    keyboard = []
    if a.status == "running":
        keyboard.append([
            InlineKeyboardButton("⏸ Pause", callback_data=f"cb_pause_{a.id}"),
            InlineKeyboardButton("🔴 Kill", callback_data=f"cb_kill_{a.id}"),
        ])
    elif a.status in ("idle", "paused"):
        keyboard.append([
            InlineKeyboardButton("▶️ Resume", callback_data=f"cb_resume_{a.id}"),
            InlineKeyboardButton("🔴 Kill", callback_data=f"cb_kill_{a.id}"),
        ])

    await update.message.reply_text(
        text, parse_mode="Markdown",
        reply_markup=InlineKeyboardMarkup(keyboard) if keyboard else None
    )


async def cmd_deploy(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    if not context.args:
        await update.message.reply_text("Usage: `/deploy <name>`", parse_mode="Markdown")
        return

    name = context.args[0]
    used = len(state.agents)
    slots = state.stake.agent_slots

    if used >= slots:
        await update.message.reply_text(
            f"⚠️ No slots available ({used}/{slots}). Upgrade tier or kill an agent."
        )
        return

    agent_id = state.next_agent_id
    state.next_agent_id += 1
    new_agent = Agent(
        id=agent_id,
        name=name,
        status="running",
        cpu=round(random.uniform(1.0, 5.0), 1),
        memory_kb=random.randint(128, 256),
        steps=0,
        uptime_s=0,
        model_type="rule_based",
        inference_us=round(random.uniform(0.25, 0.45), 2),
    )
    state.agents[agent_id] = new_agent
    add_log(f"[AGENT] deployed {name} (#{agent_id})")

    await update.message.reply_text(
        f"✅ *Agent Deployed*\n\n"
        f"🟢 *{name}* (#{agent_id})\n"
        f"Model: `rule_based`\n"
        f"Slot: {used + 1}/{slots}\n\n"
        f"_Agent is now running._",
        parse_mode="Markdown"
    )


async def cmd_kill(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    if not context.args:
        await update.message.reply_text("Usage: `/kill <id>`", parse_mode="Markdown")
        return

    try:
        agent_id = int(context.args[0])
    except ValueError:
        await update.message.reply_text("⚠️ Invalid agent ID.")
        return

    a = state.agents.get(agent_id)
    if not a:
        await update.message.reply_text(f"⚠️ Agent #{agent_id} not found.")
        return

    name = a.name
    del state.agents[agent_id]
    add_log(f"[AGENT] killed {name} (#{agent_id})")

    await update.message.reply_text(f"🔴 Agent *{name}* (#{agent_id}) terminated.", parse_mode="Markdown")


async def cmd_pause(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    if not context.args:
        await update.message.reply_text("Usage: `/pause <id>`", parse_mode="Markdown")
        return

    try:
        agent_id = int(context.args[0])
    except ValueError:
        await update.message.reply_text("⚠️ Invalid agent ID.")
        return

    a = state.agents.get(agent_id)
    if not a:
        await update.message.reply_text(f"⚠️ Agent #{agent_id} not found.")
        return

    a.status = "paused"
    a.cpu = 0.0
    add_log(f"[AGENT] paused {a.name} (#{agent_id})")
    await update.message.reply_text(f"⏸️ Agent *{a.name}* paused.", parse_mode="Markdown")


async def cmd_resume(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    if not context.args:
        await update.message.reply_text("Usage: `/resume <id>`", parse_mode="Markdown")
        return

    try:
        agent_id = int(context.args[0])
    except ValueError:
        await update.message.reply_text("⚠️ Invalid agent ID.")
        return

    a = state.agents.get(agent_id)
    if not a:
        await update.message.reply_text(f"⚠️ Agent #{agent_id} not found.")
        return

    a.status = "running"
    a.cpu = round(random.uniform(3.0, 15.0), 1)
    add_log(f"[AGENT] resumed {a.name} (#{agent_id})")
    await update.message.reply_text(f"▶️ Agent *{a.name}* resumed.", parse_mode="Markdown")


async def cmd_stake(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    s = state.stake
    text = (
        "🦞 *STAKING OVERVIEW*\n"
        "━━━━━━━━━━━━━━━━━━━━━\n\n"
        f"🏅 Tier: *{s.tier}*\n"
        f"💰 Your Stake: *{format_number(s.amount)} CLAW*\n"
        f"📈 APY: *{s.apy}%*\n"
        f"🎁 Rewards Earned: *{format_number(s.rewards_earned)} CLAW*\n"
        f"⬡ Agent Slots: *{s.agents_used}/{s.agent_slots}*\n\n"
        "━━━ *Tier Requirements* ━━━\n"
        "Scout: 1K CLAW → 12% APY (1 slot)\n"
        "Operative: 10K → 14% APY (5 slots)\n"
        "Commander: 100K → 17% APY (20 slots)\n"
        "Admiral: 1M → 22% APY (100 slots)\n\n"
        f"_Total staked network: {format_number(s.total_staked)} CLAW_"
    )
    await update.message.reply_text(text, parse_mode="Markdown")


async def cmd_wallet(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    w = state.wallet
    sol_usd = w.sol_balance * 148.32
    claw_usd = w.claw_balance * 0.03
    total_usd = sol_usd + claw_usd + w.usdc_balance + (w.bonk_balance * 0.0000195)

    text = (
        "🦞 *WALLET*\n"
        "━━━━━━━━━━━━━━━━━━━━━\n\n"
        f"📍 Address: `{w.address}`\n"
        f"🌐 Network: Solana Mainnet\n\n"
        f"◈ SOL: *{w.sol_balance:.3f}* (${sol_usd:.2f})\n"
        f"🦞 CLAW: *{format_number(w.claw_balance)}* (${claw_usd:.2f})\n"
        f"🐕 BONK: *{format_number(w.bonk_balance)}* (${w.bonk_balance * 0.0000195:.2f})\n"
        f"💵 USDC: *{w.usdc_balance:.2f}*\n\n"
        f"💎 Total: *${total_usd:,.2f}*"
    )
    await update.message.reply_text(text, parse_mode="Markdown")


async def cmd_revenue(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    rev = state.revenue_epoch
    to_stakers = int(rev * 0.7)
    to_treasury = int(rev * 0.2)
    burned = int(rev * 0.1)

    text = (
        "🦞 *REVENUE — Current Epoch*\n"
        "━━━━━━━━━━━━━━━━━━━━━\n\n"
        f"💰 Total: *{format_number(rev)} CLAW*\n\n"
        f"👥 Stakers (70%): *{format_number(to_stakers)} CLAW*\n"
        f"🏦 Treasury (20%): *{format_number(to_treasury)} CLAW*\n"
        f"🔥 Burned (10%): *{format_number(burned)} CLAW*\n\n"
        "━━━ *Sources* ━━━\n"
        f"Agent Fees: {int(rev * 0.435)} CLAW (43.5%)\n"
        f"TX Fees: {int(rev * 0.313)} CLAW (31.3%)\n"
        f"Swap Fees: {int(rev * 0.171)} CLAW (17.1%)\n"
        f"Inference Fees: {int(rev * 0.08)} CLAW (8.0%)\n"
    )
    await update.message.reply_text(text, parse_mode="Markdown")


async def cmd_governance(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    lines = ["🦞 *GOVERNANCE*\n━━━━━━━━━━━━━━━━━━━━━\n"]
    for p in state.proposals:
        total = p.votes_for + p.votes_against
        pct_for = (p.votes_for / total * 100) if total > 0 else 0
        status_emoji = {"active": "🗳", "passed": "✅", "rejected": "❌"}.get(p.status, "❓")

        lines.append(
            f"{status_emoji} *{p.id}* — {p.status.upper()}"
            + (f" ({p.days_left}d left)" if p.days_left > 0 else "") + "\n"
            f"  {p.title}\n"
            f"  For: {format_number(p.votes_for)} ({pct_for:.0f}%) | "
            f"Against: {format_number(p.votes_against)}\n"
        )

    lines.append("\n_Vote: `/vote CLS-008 y` or `/vote CLS-008 n`_")
    await update.message.reply_text("\n".join(lines), parse_mode="Markdown")


async def cmd_vote(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    if len(context.args) < 2:
        await update.message.reply_text("Usage: `/vote <proposal_id> <y/n>`", parse_mode="Markdown")
        return

    prop_id = context.args[0].upper()
    vote = context.args[1].lower()

    if vote not in ("y", "n", "yes", "no"):
        await update.message.reply_text("⚠️ Vote must be `y` or `n`.", parse_mode="Markdown")
        return

    proposal = None
    for p in state.proposals:
        if p.id == prop_id:
            proposal = p
            break

    if not proposal:
        await update.message.reply_text(f"⚠️ Proposal {prop_id} not found.")
        return

    if proposal.status != "active":
        await update.message.reply_text(f"⚠️ Proposal {prop_id} is not active.")
        return

    vote_weight = state.stake.amount
    if vote in ("y", "yes"):
        proposal.votes_for += vote_weight
        add_log(f"[GOVERNANCE] vote FOR {prop_id} — {format_number(vote_weight)} CLAW")
        await update.message.reply_text(
            f"✅ Voted *FOR* {prop_id} with *{format_number(vote_weight)} CLAW*",
            parse_mode="Markdown"
        )
    else:
        proposal.votes_against += vote_weight
        add_log(f"[GOVERNANCE] vote AGAINST {prop_id} — {format_number(vote_weight)} CLAW")
        await update.message.reply_text(
            f"❌ Voted *AGAINST* {prop_id} with *{format_number(vote_weight)} CLAW*",
            parse_mode="Markdown"
        )


async def cmd_logs(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    n = 10
    if context.args:
        try:
            n = min(int(context.args[0]), 30)
        except ValueError:
            pass

    logs = state.logs[:n]
    if not logs:
        await update.message.reply_text("📜 No logs available.")
        return

    text = "🦞 *SYSTEM LOGS*\n━━━━━━━━━━━━━━━━━━━━━\n\n"
    text += "```\n" + "\n".join(logs) + "\n```"
    await update.message.reply_text(text, parse_mode="Markdown")


async def cmd_health(update: Update, context: ContextTypes.DEFAULT_TYPE):
    if not auth_check(update.effective_user.id):
        return

    total_cpu = sum(a.cpu for a in state.agents.values())
    total_mem = sum(a.memory_kb for a in state.agents.values())
    max_mem = 1536  # 1.5 MB
    mem_pct = total_mem / max_mem * 100

    cpu_status = "🟢 OK" if total_cpu < 70 else ("🟡 WARN" if total_cpu < 90 else "🔴 CRITICAL")
    mem_status = "🟢 OK" if mem_pct < 80 else ("🟡 WARN" if mem_pct < 95 else "🔴 CRITICAL")
    rpc_status = "🟢 OK"

    text = (
        "🦞 *HEALTH CHECK*\n"
        "━━━━━━━━━━━━━━━━━━━━━\n\n"
        f"CPU: {cpu_status} ({total_cpu:.1f}%)\n"
        f"Memory: {mem_status} ({mem_pct:.0f}% — {total_mem}KB/{max_mem}KB)\n"
        f"RPC: {rpc_status} (12µs avg)\n"
        f"Agents: 🟢 {len(state.agents)} deployed\n"
        f"Security: 🟢 RBAC active\n"
        f"Training: 🟢 replay buffer OK\n\n"
        f"Overall: *{'NOMINAL' if total_cpu < 70 and mem_pct < 80 else 'DEGRADED'}*"
    )
    await update.message.reply_text(text, parse_mode="Markdown")


# ============================================================
# Callback Query Handler (inline buttons)
# ============================================================

async def callback_handler(update: Update, context: ContextTypes.DEFAULT_TYPE):
    query = update.callback_query
    await query.answer()

    data = query.data
    user_id = query.from_user.id

    if not auth_check(user_id):
        await query.edit_message_text("⛔ Unauthorized.")
        return

    if data == "cb_agents":
        lines = ["🦞 *DEPLOYED AGENTS*\n━━━━━━━━━━━━━━━━━━━━━\n"]
        for a in state.agents.values():
            emoji = STATUS_EMOJI.get(a.status, "⚪")
            lines.append(f"{emoji} *{a.name}* — `{a.status}` | CPU: `{a.cpu}%` | Mem: `{a.memory_kb}KB`")
        await query.edit_message_text("\n".join(lines), parse_mode="Markdown")

    elif data == "cb_stake":
        s = state.stake
        text = (
            f"🦞 *STAKING*\n━━━━━━━━━━━━━━━━━━━━━\n\n"
            f"Tier: *{s.tier}* | Stake: *{format_number(s.amount)} CLAW*\n"
            f"APY: *{s.apy}%* | Rewards: *{format_number(s.rewards_earned)} CLAW*"
        )
        await query.edit_message_text(text, parse_mode="Markdown")

    elif data == "cb_wallet":
        w = state.wallet
        text = (
            f"🦞 *WALLET*\n━━━━━━━━━━━━━━━━━━━━━\n\n"
            f"SOL: *{w.sol_balance:.3f}* | CLAW: *{format_number(w.claw_balance)}*\n"
            f"BONK: *{format_number(w.bonk_balance)}* | USDC: *{w.usdc_balance:.2f}*"
        )
        await query.edit_message_text(text, parse_mode="Markdown")

    elif data == "cb_logs":
        logs = state.logs[:8]
        text = "🦞 *RECENT LOGS*\n━━━━━━━━━━━━━━━━━━━━━\n\n```\n" + "\n".join(logs) + "\n```"
        await query.edit_message_text(text, parse_mode="Markdown")

    elif data.startswith("cb_pause_"):
        agent_id = int(data.split("_")[2])
        a = state.agents.get(agent_id)
        if a:
            a.status = "paused"
            a.cpu = 0.0
            add_log(f"[AGENT] paused {a.name} (#{agent_id})")
            await query.edit_message_text(f"⏸️ Agent *{a.name}* paused.", parse_mode="Markdown")

    elif data.startswith("cb_resume_"):
        agent_id = int(data.split("_")[2])
        a = state.agents.get(agent_id)
        if a:
            a.status = "running"
            a.cpu = round(random.uniform(3.0, 15.0), 1)
            add_log(f"[AGENT] resumed {a.name} (#{agent_id})")
            await query.edit_message_text(f"▶️ Agent *{a.name}* resumed.", parse_mode="Markdown")

    elif data.startswith("cb_kill_"):
        agent_id = int(data.split("_")[2])
        a = state.agents.get(agent_id)
        if a:
            name = a.name
            del state.agents[agent_id]
            add_log(f"[AGENT] killed {name} (#{agent_id})")
            await query.edit_message_text(f"🔴 Agent *{name}* terminated.", parse_mode="Markdown")


# ============================================================
# Main
# ============================================================

def main():
    if BOT_TOKEN == "YOUR_TOKEN_HERE":
        logger.error("Set TELEGRAM_BOT_TOKEN environment variable!")
        logger.error("  export TELEGRAM_BOT_TOKEN='your-token-from-botfather'")
        return

    logger.info("Starting ClawLobstars Telegram Bot...")

    app = Application.builder().token(BOT_TOKEN).build()

    # Register commands
    app.add_handler(CommandHandler("start", cmd_start))
    app.add_handler(CommandHandler("help", cmd_help))
    app.add_handler(CommandHandler("status", cmd_status))
    app.add_handler(CommandHandler("agents", cmd_agents))
    app.add_handler(CommandHandler("agent", cmd_agent_detail))
    app.add_handler(CommandHandler("deploy", cmd_deploy))
    app.add_handler(CommandHandler("kill", cmd_kill))
    app.add_handler(CommandHandler("pause", cmd_pause))
    app.add_handler(CommandHandler("resume", cmd_resume))
    app.add_handler(CommandHandler("stake", cmd_stake))
    app.add_handler(CommandHandler("wallet", cmd_wallet))
    app.add_handler(CommandHandler("revenue", cmd_revenue))
    app.add_handler(CommandHandler("governance", cmd_governance))
    app.add_handler(CommandHandler("vote", cmd_vote))
    app.add_handler(CommandHandler("logs", cmd_logs))
    app.add_handler(CommandHandler("health", cmd_health))
    app.add_handler(CallbackQueryHandler(callback_handler))

    logger.info("Bot is running. Press Ctrl+C to stop.")
    app.run_polling()


if __name__ == "__main__":
    main()
