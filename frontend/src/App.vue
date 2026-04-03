<script setup>
import { ref, onMounted, onUnmounted, computed, watch } from 'vue';

const accounts = ref([]);
const apiKey = ref(localStorage.getItem('steam_api_key') || '');
const showApiKey = ref(false);
const visiblePasswords = ref({}); // 记录每个账号密码的可见性

// 切换特定账号密码的可见性
const togglePassword = (username) => {
  visiblePasswords.value[username] = !visiblePasswords.value[username];
};

const showBulkAdd = ref(false);
const showEditPanel = ref(false);
const selectedUsernames = ref(new Set());
const editingAccount = ref(null);
const cooldownDaysInput = ref(0); // 快捷设置冷却天数
const bulkText = ref('');
const isBusy = ref(false);
const statusText = ref('就绪');
const currentTab = ref('all'); // 当前显示的页签: 'all' 或 'main'
const isSidebarCollapsed = ref(false); // 侧边栏折叠状态
let cooldownTimer = null; // 定时检查冷却
let lastSelectedUsername = null;

const contextMenu = ref({ show: false, x: 0, y: 0, account: null });

// 根据当前页签过滤账号
const filteredAccounts = computed(() => {
  if (currentTab.value === 'main') return accounts.value.filter(a => a.category === 'main');
  if (currentTab.value === 'perfect') return accounts.value.filter(a => a.category === 'perfect');
  if (currentTab.value === '5e') return accounts.value.filter(a => a.category === '5e');
  return accounts.value.filter(a => !a.category || a.category === 'normal');
});

// 自动检测并恢复到期的冷却账号
const checkCooldowns = () => {
  if (!accounts.value || accounts.value.length === 0) return;
  const now = new Date();
  let recoveredCount = 0;

  console.log('Running cooldown check at:', now.toLocaleString());

  accounts.value.forEach(acc => {
    if (acc.status === '冷却中' && acc.cooldownUntil) {
      const cooldownDate = new Date(acc.cooldownUntil);
      if (!isNaN(cooldownDate.getTime()) && cooldownDate <= now) {
        console.log(`Account ${acc.username} cooldown expired. Recovering...`);
        acc.status = '正常';
        recoveredCount++;
      }
    }
  });

  if (recoveredCount > 0) {
    statusText.value = `已自动恢复 ${recoveredCount} 个到期的冷却账号`;
    // 强制触发一次深拷贝保存，确保后端同步
    saveAccounts();
  }
};

// 自动保存逻辑：当账号列表发生任何变化（增删改）时，自动通知后端
watch(accounts, () => {
  saveAccounts();
}, { deep: true });

// 保存 API Key 到本地存储
const saveApiKey = () => {
  localStorage.setItem('steam_api_key', apiKey.value);
  statusText.value = 'API Key 已保存';
};

const openExternalLink = (url) => {
  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.postMessage({ type: 'openUrl', url: url });
  } else {
    window.open(url, '_blank');
  }
};

// 监听键盘上下键以切换选中项
const handleKeydown = (e) => {
  if (e.key === 'F12') {
    e.preventDefault();
    return;
  }

  if (['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement.tagName)) return;
  if (showEditPanel.value || showBulkAdd.value) return;
  if (filteredAccounts.value.length === 0) return;

  if (e.ctrlKey && e.key === 'a') {
    e.preventDefault();
    selectAllAccounts();
    return;
  }

  const lastSelected = Array.from(selectedUsernames.value).pop();
  const currentIndex = filteredAccounts.value.findIndex(a => a.username === lastSelected);

  if (e.key === 'ArrowDown') {
    const nextIndex = currentIndex === -1 ? 0 : (currentIndex + 1) % filteredAccounts.value.length;
    selectAccount(filteredAccounts.value[nextIndex].id, e.shiftKey);
    e.preventDefault();
    scrollToSelected();
  } else if (e.key === 'ArrowUp') {
    const prevIndex = currentIndex === -1 ? filteredAccounts.value.length - 1 : (currentIndex - 1 + filteredAccounts.value.length) % filteredAccounts.value.length;
    selectAccount(filteredAccounts.value[prevIndex].id, e.shiftKey);
    e.preventDefault();
    scrollToSelected();
  } else if (e.key === 'Enter' && selectedUsernames.value.size > 0) {
    handleEditSelected();
    e.preventDefault();
  } else if (e.key === 'Delete' || e.key === 'Del' || (e.ctrlKey && e.key.toLowerCase() === 'd')) {
    if (selectedUsernames.value.size > 0) {
      handleBulkDelete();
      e.preventDefault();
    }
  } else if (e.key === 'Escape') {
    clearSelection();
    showEditPanel.value = false;
    showBulkAdd.value = false;
  }
};

const selectAllAccounts = () => {
  selectedUsernames.value = new Set(filteredAccounts.value.map(a => a.id));
};

const clearSelection = () => {
  selectedUsernames.value = new Set();
};

const scrollToSelected = () => {
  setTimeout(() => {
    const selectedRow = document.querySelector('.is-selected');
    if (selectedRow) {
      selectedRow.scrollIntoView({ block: 'nearest', behavior: 'smooth' });
    }
  }, 10);
};

onMounted(() => {
  window.addEventListener('keydown', handleKeydown);
  window.addEventListener('click', closeContextMenu);
  window.addEventListener('wheel', (e) => {
    if (e.ctrlKey) e.preventDefault();
  }, { passive: false });

  // 每分钟自动检查一次冷却状态
  cooldownTimer = setInterval(checkCooldowns, 60000);

  // 监听来自 C++ 的消息
  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.addEventListener('message', event => {
      console.log('Received message from C++', event.data);
      if (event.data.type === 'setAccounts') {
        accounts.value = event.data.accounts || [];
        statusText.value = `已加载 ${accounts.value.length} 个账号`;
        // 加载后立即检查一次
        checkCooldowns();
      }
    });
    // 向 C++ 请求账号数据
    window.chrome.webview.postMessage({ type: 'getAccounts' });
  } else {
    // 本地开发测试数据 (增加过期时间用于验证自动恢复)
    accounts.value = [
      { id: 'debug_1', username: 'testuser1', alias: '测试账号 1', status: '正常', steamId: '76561198000000001' },
      { 
        id: 'debug_2',
        username: 'testuser2', 
        alias: '测试账号 2', 
        status: '冷却中', 
        steamId: '76561198000000002',
        cooldownUntil: '2020-01-01T00:00' // 一个明显过期的时间
      }
    ];
    checkCooldowns();
  }
});

onUnmounted(() => {
  window.removeEventListener('keydown', handleKeydown);
  window.removeEventListener('click', closeContextMenu);
  if (cooldownTimer) clearInterval(cooldownTimer);
});

const saveAccounts = () => {
  if (window.chrome && window.chrome.webview) {
    // 使用深拷贝确保发送的是纯净的 JS 对象而非 Vue Proxy
    const data = JSON.parse(JSON.stringify(accounts.value));
    window.chrome.webview.postMessage({ 
      type: 'saveAccounts', 
      accounts: data 
    });
  }
};

const handleBulkAdd = () => {
  if (!bulkText.value.trim()) return;
  
  const lines = bulkText.value.split('\n');
  const newEntries = [];
  
  lines.forEach(line => {
    const trimmedLine = line.trim();
    if (!trimmedLine) return;

    // 优先匹配 username----password 格式
    let username = '';
    let password = '';
    let alias = '';

    if (trimmedLine.includes('----')) {
      const parts = trimmedLine.split('----');
      username = parts[0].trim();
      password = parts[1] ? parts[1].trim() : '';
      alias = username;
    } else {
      // 兼容空格、逗号等分隔符
      const parts = trimmedLine.split(/[,，\s]+/);
      username = parts[0].trim();
      alias = parts[1] ? parts[1].trim() : username;
    }
    
    if (username) {
      // 使用时间戳+随机数生成唯一ID，允许同名账号存在
      const uniqueId = 'id_' + Date.now() + '_' + Math.random().toString(36).substr(2, 9);
      newEntries.push({
        id: uniqueId,
        username: username,
        password: password,
        alias: alias,
        status: '正常',
        steamId: '',
        category: currentTab.value === 'all' ? 'normal' : currentTab.value
      });
    }
  });
  
  if (newEntries.length > 0) {
    accounts.value = [...accounts.value, ...newEntries];
    saveAccounts(); // 保存到本地文件
    statusText.value = `成功添加 ${newEntries.length} 个账号`;
  }
  
  bulkText.value = '';
  showBulkAdd.value = false;
};

const accountsToDelete = ref(null); // 用于批量删除

const deleteAccount = (account) => {
  accountsToDelete.value = [account]; // 单个删除转化为数组
};

const handleBulkDelete = () => {
  if (selectedUsernames.value.size === 0) {
    statusText.value = '请先选择要删除的账号';
    return;
  }
  const toDelete = accounts.value.filter(a => selectedUsernames.value.has(a.id));
  accountsToDelete.value = toDelete;
};

const confirmDelete = () => {
  if (accountsToDelete.value && accountsToDelete.value.length > 0) {
    const deletedIds = accountsToDelete.value.map(a => a.id);
    accounts.value = accounts.value.filter(a => !deletedIds.includes(a.id));
    
    // 清除选中的已删除项
    const newSelection = new Set(selectedUsernames.value);
    deletedIds.forEach(id => newSelection.delete(id));
    selectedUsernames.value = newSelection;
    
    saveAccounts();
    statusText.value = `成功删除 ${deletedIds.length} 个账号`;
    accountsToDelete.value = null;
  }
};

const login = (account) => {
  if (!account) return;

  statusText.value = `正在登录 ${account.username}...`;
  console.log('Sending login request for:', account.username, 'Password length:', account.password?.length || 0);

  isBusy.value = true;
  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.postMessage({ 
      type: 'login', 
      username: account.username,
      password: account.password || ''
    });
  }
  // 模拟登录后恢复状态
  setTimeout(() => {
    isBusy.value = false;
    statusText.value = '登录指令已发送';
  }, 2000);
};

const handleWindowControl = (action) => {
  if (window.chrome && window.chrome.webview) {
    window.chrome.webview.postMessage({ type: 'windowControl', action });
  }
};

const handleDragStart = (e) => {
  if (e.button === 0) {
    if (e.target.closest('.win-btn')) return;
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage({ type: 'windowControl', action: 'startDrag' });
    }
  }
};

const openProfile = (steamId) => {
  if (steamId) {
    openExternalLink(`https://steamcommunity.com/profiles/${steamId}`);
  }
};

const formatCooldown = (dateStr) => {
  if (!dateStr) return '';
  try {
    const d = new Date(dateStr);
    if (isNaN(d.getTime())) return dateStr; // 处理非标准日期字符串
    // 简短显示：月/日 时:分
    return d.toLocaleString([], { month: 'numeric', day: 'numeric', hour: '2-digit', minute: '2-digit' });
  } catch(e) {
    return '';
  }
};

const applyCooldownDays = () => {
  if (!editingAccount.value) return;
  const now = new Date();
  const days = parseFloat(cooldownDaysInput.value || 0);
  if (days <= 0) return;
  
  // 计算结束时间
  now.setTime(now.getTime() + days * 24 * 60 * 60 * 1000);
  
  // 转换为 datetime-local 要求的格式: YYYY-MM-DDTHH:mm
  const year = now.getFullYear();
  const month = String(now.getMonth() + 1).padStart(2, '0');
  const day = String(now.getDate()).padStart(2, '0');
  const hours = String(now.getHours()).padStart(2, '0');
  const minutes = String(now.getMinutes()).padStart(2, '0');
  
  editingAccount.value.cooldownUntil = `${year}-${month}-${day}T${hours}:${minutes}`;
  statusText.value = `已设置 ${days} 天冷却`;
};

const selectAccount = (id, event) => {
  const isCtrl = event?.ctrlKey;
  const isShift = event?.shiftKey;

  if (isShift && lastSelectedUsername) {
    const filtered = filteredAccounts.value;
    const lastIndex = filtered.findIndex(a => a.id === lastSelectedUsername);
    const currentIndex = filtered.findIndex(a => a.id === id);
    if (lastIndex !== -1 && currentIndex !== -1) {
      const start = Math.min(lastIndex, currentIndex);
      const end = Math.max(lastIndex, currentIndex);
      const newSelection = new Set(selectedUsernames.value);
      for (let i = start; i <= end; i++) {
        newSelection.add(filtered[i].id);
      }
      selectedUsernames.value = newSelection;
      lastSelectedUsername = id;
      return;
    }
  }
  if (isCtrl) {
    if (selectedUsernames.value.has(id)) {
      const newSelection = new Set(selectedUsernames.value);
      newSelection.delete(id);
      selectedUsernames.value = newSelection;
    } else {
      const newSelection = new Set(selectedUsernames.value);
      newSelection.add(id);
      selectedUsernames.value = newSelection;
    }
  } else {
    selectedUsernames.value = new Set([id]);
  }
  lastSelectedUsername = id;
};

const handleEditSelected = () => {
  if (selectedUsernames.value.size === 0) {
    statusText.value = '请先点击表格行选择一个账号';
    return;
  }
  const firstSelectedId = Array.from(selectedUsernames.value)[0];
  const acc = accounts.value.find(a => a.id === firstSelectedId);
  if (acc) {
    editingAccount.value = { ...acc };
    cooldownDaysInput.value = 0;
    showEditPanel.value = true;
    showBulkAdd.value = false;
  }
};

const saveEdit = () => {
  if (!editingAccount.value) return;
  const oldId = Array.from(selectedUsernames.value)[0];
  const index = accounts.value.findIndex(a => a.id === oldId);
  if (index !== -1) {
    // 如果选择了“冷却中”，但没有设置时间或时间已过期，则自动恢复为“正常”
    if (editingAccount.value.status === '冷却中') {
      const now = new Date();
      if (!editingAccount.value.cooldownUntil) {
        editingAccount.value.status = '正常';
      } else {
        const cooldownDate = new Date(editingAccount.value.cooldownUntil);
        if (isNaN(cooldownDate.getTime()) || cooldownDate <= now) {
          editingAccount.value.status = '正常';
          editingAccount.value.cooldownUntil = ''; // 清理过期的具体日期
        }
      }
    }

    accounts.value[index] = { ...editingAccount.value };
    const newSelection = new Set(selectedUsernames.value);
    newSelection.delete(oldId);
    newSelection.add(editingAccount.value.id);
    selectedUsernames.value = newSelection;
    saveAccounts();
    showEditPanel.value = false;
    statusText.value = '账号信息已更新';
  }
};

const openContextMenu = (e, account) => {
  e.preventDefault();
  e.stopPropagation();
  selectedUsernames.value = new Set([account.id]);
  lastSelectedUsername = account.id;
  contextMenu.value = {
    show: true,
    x: e.clientX,
    y: e.clientY,
    account: account
  };
};

const closeContextMenu = (e) => {
  if (e && e.target.closest('.context-menu')) return;
  contextMenu.value.show = false;
};

const handleContextAction = (action) => {
  const acc = contextMenu.value.account;
  if (!acc) return;

  closeContextMenu();

  switch (action) {
    case 'login':
      login(acc);
      break;
    case 'edit':
      selectedUsernames.value = new Set([acc.id]);
      lastSelectedUsername = acc.id;
      editingAccount.value = { ...acc };
      cooldownDaysInput.value = 0;
      showEditPanel.value = true;
      showBulkAdd.value = false;
      break;
    case 'delete':
      deleteAccount(acc);
      break;
    case 'copyUsername':
      navigator.clipboard.writeText(acc.username);
      statusText.value = `已复制: ${acc.username}`;
      break;
    case 'copyPassword':
      if (acc.password) {
        navigator.clipboard.writeText(acc.password);
        statusText.value = '密码已复制';
      }
      break;
  }
};
</script>

<template>
  <div class="app-container" :class="{ 'is-busy': isBusy }">
    <!-- 自定义标题栏 -->
    <div class="custom-titlebar" 
         @dblclick="handleWindowControl('maximize')"
         @mousedown="handleDragStart">
      <div class="title-content">
        <span class="app-title">Steam Account Manager</span>
      </div>
      <div class="window-controls">
        <button class="win-btn" @click="handleWindowControl('minimize')" title="最小化">
          <svg viewBox="0 0 10 10"><path d="M0 5h10v1H0z" fill="white"/></svg>
        </button>
        <button class="win-btn" @click="handleWindowControl('maximize')" title="最大化/还原">
          <svg viewBox="0 0 10 10"><path d="M0 0v10h10V0H0zm1 1h8v8H1V1z" fill="white"/></svg>
        </button>
        <button class="win-btn close" @click="handleWindowControl('close')" title="关闭">
          <svg viewBox="0 0 10 10"><path d="M0 0l10 10M10 0L0 10" stroke="white" stroke-width="1.2"/></svg>
        </button>
      </div>
    </div>

    <div class="layout-body">
      <!-- 左侧导航侧边栏 -->
      <aside class="sidebar" :class="{ 'collapsed': isSidebarCollapsed }">
        <div class="sidebar-header">
          <span v-if="!isSidebarCollapsed">SAM</span>
          <button @click="isSidebarCollapsed = !isSidebarCollapsed" class="toggle-sidebar-btn">
            {{ isSidebarCollapsed ? '→' : '←' }}
          </button>
        </div>
        <nav class="nav-menu">
          <button @click="currentTab = 'main'" :class="['nav-item', { active: currentTab === 'main' }]" :title="isSidebarCollapsed ? '主账号' : ''">
            <span class="icon">⭐</span>
            <span class="label">主账号</span>
          </button>
          <button @click="currentTab = 'perfect'" :class="['nav-item', { active: currentTab === 'perfect' }]" :title="isSidebarCollapsed ? '完美账号' : ''">
            <span class="icon"><img src="./assets/perfect.png" style="width: 16px; height: 16px; object-fit: contain; vertical-align: middle;" /></span>
            <span class="label">完美账号</span>
          </button>
          <button @click="currentTab = '5e'" :class="['nav-item', { active: currentTab === '5e' }]" :title="isSidebarCollapsed ? '5E账号' : ''">
            <span class="icon"><img src="./assets/5E.png" style="width: 16px; height: 16px; object-fit: contain; vertical-align: middle;" /></span>
            <span class="label">5E账号</span>
          </button>
          <button @click="currentTab = 'all'" :class="['nav-item', { active: currentTab === 'all' }]" :title="isSidebarCollapsed ? '普通账号' : ''">
            <span class="icon">📋</span>
            <span class="label">普通账号</span>
          </button>
        </nav>
        <div class="sidebar-footer" v-if="!isSidebarCollapsed">
          v1.0.0
        </div>
      </aside>

      <!-- 右侧主区域 -->
      <div class="main-content">
        <!-- 顶部工具栏 -->
        <header class="toolbar">
          <div class="view-title">
            <template v-if="currentTab === 'main'">⭐ 我的主账号</template>
            <template v-else-if="currentTab === 'perfect'"><img src="./assets/perfect.png" style="width: 18px; height: 18px; object-fit: contain; vertical-align: text-bottom; margin-right: 6px;" />完美平台账号</template>
            <template v-else-if="currentTab === '5e'"><img src="./assets/5E.png" style="width: 18px; height: 18px; object-fit: contain; vertical-align: text-bottom; margin-right: 6px;" />5E对战平台账号</template>
            <template v-else>📋 普通账号列表</template>
          </div>
          <div class="spacer"></div>
          <button @click="showBulkAdd = !showBulkAdd; showEditPanel = false" class="btn primary">➕批量添加➕</button>
          <button @click="handleEditSelected" class="btn">✏️更改选中✏️</button>
          <button @click="handleBulkDelete" class="btn" style="color: #ff4d4f; border-color: #ff4d4f;">🗑️删除选中🗑️</button>
          <button v-if="apiKey" @click="statusText = '功能开发中...'" class="btn">🔄批量更新昵称 (API)🔄</button>
          <button v-if="apiKey" @click="statusText = '功能开发中...'" class="btn">🛡️更新封禁状态 (API)🛡️</button>
        </header>

        <!-- 批量添加面板 -->
        <Transition name="slide-down">
          <div v-if="showBulkAdd" class="bulk-panel">
            <textarea v-model="bulkText" placeholder="每行一个账号，格式：用户名----密码 或 用户名"></textarea>
            <div class="bulk-actions">
              <button @click="handleBulkAdd" class="btn success">确认添加</button>
              <button @click="showBulkAdd = false" class="btn">取消</button>
            </div>
          </div>
        </Transition>

        <!-- 主表格区域 -->
        <main class="table-container">
          <div class="table-section">
            <table class="data-grid">
              <thead>
                <tr>
                  <th style="width: 140px">用户名</th>
                  <th style="width: 140px">密码</th>
                  <th style="width: 110px">状态</th>
                  <th>备注 / 昵称</th>
                  <th style="width: 180px">SteamID64</th>
                  <th style="width: 110px">操作</th>
                </tr>
              </thead>
              <tbody>
                <tr v-for="account in filteredAccounts" :key="account.id || account.username"
                    :class="['row-' + account.status, { 'is-selected': selectedUsernames.has(account.id) }]"
                    @click="selectAccount(account.id, $event)"
                    @contextmenu="openContextMenu($event, account)">
                  <td>
                    <span v-if="account.category === 'main'" style="color: #FFD700; margin-right: 4px;" title="主账号">⭐</span>
                    <img v-else-if="account.category === 'perfect'" src="./assets/perfect.png" style="width: 14px; height: 14px; object-fit: contain; vertical-align: middle; margin-right: 4px;" title="完美账号" />
                    <img v-else-if="account.category === '5e'" src="./assets/5E.png" style="width: 14px; height: 14px; object-fit: contain; vertical-align: middle; margin-right: 4px;" title="5E账号" />
                    {{ account.username }}
                  </td>
                  <td>
                    <div class="input-wrapper table-pwd-wrapper">
                      <input v-model="account.password" @change="saveAccounts" 
                             :type="visiblePasswords[account.username] ? 'text' : 'password'" 
                             autocomplete="new-password" spellcheck="false"
                             class="edit-input pwd" placeholder="无密码" />
                      <button class="toggle-btn" @click="togglePassword(account.username)">
                        <svg v-if="visiblePasswords[account.username]" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                          <path d="M2 12C2 12 5 5 12 5C19 5 22 12 22 12C22 12 19 19 12 19C5 19 2 12 2 12Z" />
                          <circle cx="12" cy="12" r="3" />
                          <line x1="3" y1="3" x2="21" y2="21" />
                        </svg>
                        <svg v-else width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                          <path d="M2 12C2 12 5 5 12 5C19 5 22 12 22 12C22 12 19 19 12 19C5 19 2 12 2 12Z" />
                          <circle cx="12" cy="12" r="3" />
                        </svg>
                      </button>
                    </div>
                  </td>
                  <td>
                    <span :class="['status-badge', account.status === '正常' ? 'status-ok' : 'status-warn']">
                      {{ account.status }}
                      <span v-if="account.status === '冷却中' && account.cooldownUntil" class="cooldown-text">
                        ({{ formatCooldown(account.cooldownUntil) }})
                      </span>
                    </span>
                  </td>
                  <td>
                    <input v-model="account.alias" class="edit-input" placeholder="点击编辑备注..." />
                  </td>
                  <td @click="openProfile(account.steamId)" class="link-cell">{{ account.steamId || '---' }}</td>
                  <td>
                    <div class="row-actions">
                      <button @click="login(account)" class="btn-sm login">登录</button>
                      <button @click="deleteAccount(account)" class="btn-sm delete">删除</button>
                    </div>
                  </td>
                </tr>
                <tr v-if="filteredAccounts.length === 0">
                  <td colspan="6" style="text-align: center; padding: 40px; color: #888;">
                    {{ currentTab === 'main' ? '暂无主账号' : (currentTab === 'perfect' ? '暂无完美账号' : (currentTab === '5e' ? '暂无5E账号' : '暂无普通账号')) }}，请点击上方批量添加
                  </td>
                </tr>
              </tbody>
            </table>
          </div>
        </main>
      </div>
    </div>

    <!-- 弹窗：编辑选中账号 -->
    <Transition name="modal-fade">
      <div v-if="showEditPanel && editingAccount" class="modal-overlay" @click.self="showEditPanel = false">
        <div class="modal-dialog edit-panel">
          <h3 class="modal-title">✏️ 编辑账号信息</h3>
          <div class="edit-grid">
            <div class="input-group">
              <label>用户名:</label>
              <input v-model="editingAccount.username" type="text" />
            </div>
            <div class="input-group">
              <label>密码:</label>
              <input v-model="editingAccount.password" type="text" autocomplete="off" placeholder="留空则不修改" />
            </div>
            <div class="input-group">
              <label>备注 / 昵称:</label>
              <input v-model="editingAccount.alias" type="text" />
            </div>
            <div class="input-group">
              <label>CS2 状态:</label>
              <select v-model="editingAccount.status">
                <option value="正常">正常</option>
                <option value="冷却中">冷却中</option>
                <option value="封禁">封禁</option>
                <option value="未知">未知</option>
              </select>
            </div>
            <div class="input-group" v-if="editingAccount.status === '冷却中'">
              <label>设置冷却时长 (天):</label>
              <div class="cooldown-setter-box">
                <input v-model="cooldownDaysInput" type="number" min="0" step="0.5" placeholder="输入天数" />
                <button @click="applyCooldownDays" class="btn-sm login">应用</button>
              </div>
            </div>
            <div class="input-group" v-if="editingAccount.status === '冷却中'">
              <label>确切结束时间:</label>
              <input v-model="editingAccount.cooldownUntil" type="datetime-local" />
            </div>
            <div class="input-group" style="grid-column: span 2;">
              <label>SteamID64 (用于打开个人主页):</label>
              <input v-model="editingAccount.steamId" type="text" />
            </div>
          </div>
          <div class="edit-actions">
            <button @click="saveEdit" class="btn success">✅ 保存修改</button>
            <button @click="showEditPanel = false" class="btn">❌ 取消</button>
          </div>
        </div>
      </div>
    </Transition>

    <!-- 弹窗：确认删除账号 -->
    <Transition name="modal-fade">
      <div v-if="accountsToDelete && accountsToDelete.length > 0" class="modal-overlay" @click.self="accountsToDelete = null">
        <div class="modal-dialog" style="width: 320px; text-align: center;">
          <h3 class="modal-title" style="color: #FF4D4F; margin-bottom: 10px;">⚠️ 确认删除</h3>
          <p style="margin: 20px 0; font-size: 14px; color: #ccc;">
            <template v-if="accountsToDelete.length === 1">
              确定要删除账号 <br/><br/>
              <strong style="color: white; font-size: 18px;">{{ accountsToDelete[0].username }}</strong> <br/><br/>
              吗？
            </template>
            <template v-else>
              确定要批量删除 <br/><br/>
              <strong style="color: white; font-size: 18px;">已选中的 {{ accountsToDelete.length }} 个账号</strong> <br/><br/>
              吗？
            </template>
          </p>
          <div class="actions" style="display: flex; justify-content: center; gap: 15px; margin-top: 25px;">
            <button @click="confirmDelete" class="btn" style="background-color: #FF4D4F; color: white; border-color: #FF4D4F; min-width: 100px;">确定删除</button>
            <button @click="accountsToDelete = null" class="btn" style="min-width: 100px;">取消</button>
          </div>
        </div>
      </div>
    </Transition>

    <!-- 右键菜单 -->
    <Transition name="context-menu">
      <div v-if="contextMenu.show"
           class="context-menu"
           :style="{ left: contextMenu.x + 'px', top: contextMenu.y + 'px' }"
           @click.stop>
        <div class="context-menu-item" @click="handleContextAction('login')">🚀 登录</div>
        <div class="context-menu-item" @click="handleContextAction('edit')">✏️ 编辑</div>
        <div class="context-menu-divider"></div>
        <div class="context-menu-item" @click="handleContextAction('copyUsername')">📋 复制用户名</div>
        <div class="context-menu-item" @click="handleContextAction('copyPassword')">🔐 复制密码</div>
        <div class="context-menu-divider"></div>
        <div class="context-menu-item danger" @click="handleContextAction('delete')">🗑️ 删除</div>
      </div>
    </Transition>

    <!-- 底部状态栏 -->
    <footer class="status-bar">
      <span>{{ statusText }}</span>
      <span class="right">GitHub: noob-xiaoyu/SteamAccountManager</span>
    </footer>
  </div>
</template>

<style scoped>
/* 表格行选择样式 */
.is-selected {
  background-color: #3E3E42 !important;
  /* 使用 outline 产生内描边效果，不会抖动且能显示完整边框 */
  outline: 1px solid #007ACC;
  outline-offset: -1px;
}

/* 弹窗遮罩层 */
.modal-overlay {
  position: fixed;
  top: 0;
  left: 0;
  width: 100vw;
  height: 100vh;
  background-color: rgba(0, 0, 0, 0.7);
  display: flex;
  justify-content: center;
  align-items: center;
  z-index: 1000;
  backdrop-filter: blur(2px);
}

/* 弹窗主体 */
.modal-dialog {
  background-color: #333337;
  border: 1px solid #444;
  border-radius: 8px;
  padding: 20px;
  width: 500px;
  max-width: 90vw;
  box-shadow: 0 10px 25px rgba(0, 0, 0, 0.5);
}

.modal-title {
  margin-top: 0;
  margin-bottom: 20px;
  font-size: 16px;
  font-weight: 600;
  color: #007ACC;
}

.edit-panel {
  /* 移除之前的相对布局样式 */
}

.edit-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 15px;
  margin-bottom: 15px;
}

.input-group {
  display: flex;
  flex-direction: column;
}

.input-group label {
  font-size: 12px;
  color: #aaa;
  margin-bottom: 5px;
}

.input-group input, .input-group select {
  background-color: #252526;
  border: 1px solid #454545;
  color: white;
  padding: 8px;
  border-radius: 4px;
}

.input-group input:focus {
  border-color: #007ACC;
  outline: none;
}

/* 强制穿透 Scoped Style 以隐藏所有浏览器/Edge 自带的图标按钮 */
:global(input::-ms-reveal),
:global(input::-ms-clear),
:global(input::-webkit-contacts-auto-fill-button),
:global(input::-webkit-credentials-auto-fill-button),
:global(input::-webkit-search-cancel-button),
:global(input::-webkit-search-results-button) {
  display: none !important;
  visibility: hidden !important;
  pointer-events: none !important;
  width: 0 !important;
  height: 0 !important;
}

.cooldown-setter-box {
  display: flex;
  gap: 5px;
}

.cooldown-setter-box input {
  flex: 1;
}

.edit-actions {
  display: flex;
  gap: 10px;
  justify-content: flex-end;
}

.app-container {
  display: flex;
  flex-direction: column;
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: #2D2D30;
  color: white;
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  overflow: hidden;
  box-sizing: border-box;
}

/* 自定义标题栏 */
.custom-titlebar {
  height: 32px;
  background-color: #1e1e1e;
  display: flex;
  justify-content: space-between;
  align-items: center;
  user-select: none;
  z-index: 9999;
  border-bottom: 1px solid #333;
  flex-shrink: 0;
}

.title-content {
  display: flex;
  align-items: center;
  padding-left: 12px;
  color: #969696;
  font-size: 12px;
  pointer-events: none;
}

.window-controls {
  display: flex;
  height: 100%;
}

.win-btn {
  width: 46px;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  background: transparent;
  border: none;
  cursor: pointer;
  transition: background-color 0.1s;
}

.win-btn:hover {
  background-color: #333333;
}

.win-btn.close:hover {
  background-color: #e81123;
}

.win-btn svg {
  width: 10px;
  height: 10px;
}

.layout-body {
  display: flex;
  flex: 1;
  overflow: hidden;
}

/* 侧边栏样式 */
.sidebar {
  width: 160px;
  background-color: #252526;
  border-right: 1px solid #333;
  display: flex;
  flex-direction: column;
  flex-shrink: 0;
  transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1);
  overflow-x: hidden; /* 关键：宽度不足时隐藏溢出的文字内容 */
}

.sidebar.collapsed {
  width: 50px;
}

.sidebar-header {
  padding: 0 15px;
  font-weight: bold;
  font-size: 18px;
  color: #007ACC;
  display: flex;
  justify-content: space-between;
  align-items: center;
  border-bottom: 1px solid #333;
  height: 50px;
  white-space: nowrap;
  overflow: hidden;
}

.sidebar.collapsed .sidebar-header {
  justify-content: center;
  padding: 0;
}

.toggle-sidebar-btn {
  background: transparent;
  border: none;
  color: #888;
  cursor: pointer;
  padding: 5px;
  font-size: 14px;
  border-radius: 4px;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}

.toggle-sidebar-btn:hover {
  background-color: #3e3e42;
  color: #fff;
}

.nav-menu {
  flex: 1;
  padding: 10px 0;
}

.nav-item {
  width: 160px; /* 固定宽度，与侧边栏展开宽度一致 */
  height: 40px;
  display: flex;
  align-items: center;
  background: transparent;
  border: none;
  color: #aaa;
  cursor: pointer;
  transition: background-color 0.2s, color 0.2s;
  font-size: 14px;
  padding: 0;
  text-align: left;
}

.nav-item:hover {
  background-color: #2a2d2e;
  color: #ccc;
}

.nav-item.active {
  background-color: #37373d;
  color: #fff;
  box-shadow: inset 3px 0 0 #007ACC;
}

.nav-item .icon {
  font-size: 16px;
  width: 50px; /* 宽度与折叠后的侧边栏一致 */
  min-width: 50px;
  display: flex;
  justify-content: center;
  align-items: center;
  flex-shrink: 0;
}

.nav-item .label {
  white-space: nowrap;
  opacity: 1;
  transition: opacity 0.2s;
}

.sidebar.collapsed .nav-item .label {
  opacity: 0; /* 折叠时不仅被遮挡，还完全透明，增强视觉效果 */
}

.sidebar-footer {
  padding: 10px;
  text-align: center;
  font-size: 10px;
  color: #555;
  border-top: 1px solid #333;
}

/* 主内容区 */
.main-content {
  flex: 1;
  display: flex;
  flex-direction: column;
  overflow: hidden;
}

.view-title {
  font-size: 14px;
  font-weight: 600;
  color: #ddd;
  white-space: nowrap;      /* 强制标题不换行 */
  overflow: hidden;         /* 溢出隐藏 */
  text-overflow: ellipsis;  /* 溢出显示省略号 */
  max-width: 200px;         /* 限制最大宽度，防止挤压右侧按钮 */
}

.spacer {
  flex: 1;
}

/* 工具栏 */
.toolbar {
  display: flex;
  gap: 10px;
  padding: 10px 15px;
  background-color: #252526;
  border-bottom: 1px solid #3e3e42;
}

/* 自定义标题栏 */
.custom-titlebar {
  height: 32px;
  background-color: #1e1e1e;
  display: flex;
  justify-content: space-between;
  align-items: center;
  user-select: none;
  z-index: 9999;
}

.title-content {
  display: flex;
  align-items: center;
  padding-left: 10px;
  color: #969696;
  font-size: 12px;
  pointer-events: none;
}

.window-controls {
  display: flex;
  height: 100%;
}

.win-btn {
  width: 46px;
  height: 100%;
  display: flex;
  align-items: center;
  justify-content: center;
  background: transparent;
  border: none;
  color: #ffffff;
  cursor: pointer;
  transition: background-color 0.2s;
}

.win-btn:hover {
  background-color: #333333;
}

.win-btn.close:hover {
  background-color: #e81123;
}

.win-btn svg {
  width: 10px;
  height: 100%;
  fill: none;
  stroke: currentColor;
  stroke-width: 1;
}

.btn {
  background-color: #3E3E42;
  color: white;
  border: 1px solid #555;
  padding: 0 12px;
  cursor: pointer;
  font-size: 13px;
  border-radius: 3px;
  height: 24px;
  display: flex;
  align-items: center;
  white-space: nowrap; /* 强制按钮文字不换行 */
  flex-shrink: 0;      /* 防止按钮在空间不足时被挤压变形 */
}

.btn:hover {
  background-color: #555557;
}

.btn.primary {
  border-color: #007ACC;
}

.btn.success {
  background-color: #2d5a27;
}

/* API Key 区 */
.api-key-area {
  height: 34px;
  padding: 0 18px;
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 13px;
  background-color: #252526;
  border-bottom: 1px solid #333;
}

.input-wrapper {
  position: relative;
  display: flex;
  align-items: center;
}

.api-key-area .input-wrapper {
  width: 300px;
}

.api-key-area input {
  background-color: #3E3E42;
  border: 1px solid #555;
  color: white;
  padding: 0 30px 0 8px; /* 预留右侧图标空间 */
  width: 100%;
  height: 24px;
}

.toggle-btn {
  position: absolute;
  right: 4px;
  background: none;
  border: none;
  color: #888;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 2px;
  transition: color 0.2s;
}

.toggle-btn:hover {
  color: #CCC;
}

.table-pwd-wrapper {
  width: 130px;
}

.table-pwd-wrapper input {
  padding-right: 25px !important;
}

.api-key-area a {
  color: #007ACC;
  text-decoration: none;
}

/* 批量面板 */
.bulk-panel {
  padding: 10px;
  background-color: #333;
  border-bottom: 1px solid #444;
}

.bulk-panel textarea {
  width: 100%;
  height: 80px;
  background-color: #1e1e1e;
  color: #dcdcdc;
  border: 1px solid #555;
  padding: 5px;
  resize: vertical;
  margin-bottom: 5px;
}

.bulk-panel textarea::-webkit-scrollbar {
  width: 8px;
}

.bulk-panel textarea::-webkit-scrollbar-thumb {
  background: #555;
  border-radius: 4px;
}

.bulk-panel textarea::-webkit-scrollbar-track {
  background: #2d2d30;
}

.bulk-panel {
  overflow: hidden;
}

.slide-down-enter-active,
.slide-down-leave-active {
  transition: all 0.3s ease;
}

.slide-down-enter-from,
.slide-down-leave-to {
  opacity: 0;
  max-height: 0;
  padding-top: 0;
  padding-bottom: 0;
  margin-bottom: 0;
}

.slide-down-enter-to,
.slide-down-leave-from {
  opacity: 1;
  max-height: 200px;
}

.modal-fade-enter-active,
.modal-fade-leave-active {
  transition: opacity 0.2s ease;
}

.modal-fade-enter-from,
.modal-fade-leave-to {
  opacity: 0;
}

.modal-fade-enter-active .modal-dialog,
.modal-fade-leave-active .modal-dialog {
  transition: transform 0.2s ease;
}

.modal-fade-enter-from .modal-dialog,
.modal-fade-leave-to .modal-dialog {
  transform: scale(0.95);
}

.context-menu {
  position: fixed;
  background-color: #2d2d30;
  border: 1px solid #444;
  border-radius: 6px;
  padding: 5px 0;
  min-width: 160px;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.4);
  z-index: 10000;
}

.context-menu-item {
  padding: 8px 14px;
  cursor: pointer;
  font-size: 13px;
  color: #ccc;
  display: flex;
  align-items: center;
  gap: 8px;
}

.context-menu-item:hover {
  background-color: #094771;
  color: white;
}

.context-menu-item.danger:hover {
  background-color: #8b0000;
}

.context-menu-divider {
  height: 1px;
  background-color: #444;
  margin: 5px 0;
}

.context-menu-enter-active,
.context-menu-leave-active {
  transition: opacity 0.15s ease, transform 0.15s ease;
}

.context-menu-enter-from,
.context-menu-leave-to {
  opacity: 0;
  transform: scale(0.95);
}

.bulk-actions {
  display: flex;
  gap: 10px;
  justify-content: flex-end;
}

/* 表格样式 */
.table-container {
  flex: 1;
  overflow: auto; /* 同时支持垂直和水平滚动 */
  padding: 0 10px;
}

.data-grid {
  width: 100%;
  border-collapse: collapse;
  font-size: 13px;
  table-layout: fixed; /* 固定布局，防止自动撑开 */
}

.data-grid th, .data-grid td {
  white-space: nowrap;      /* 禁止换行 */
  overflow: hidden;         /* 隐藏溢出 */
  text-overflow: ellipsis;  /* 溢出显示省略号 */
  padding: 0 8px;
}

.data-grid th {
  background-color: #3E3E42;
  text-align: left;
  height: 34px;
  border: 1px solid #555;
  position: sticky;
  top: 0;
}

.data-grid td {
  height: 34px;
  border-bottom: 1px solid #444;
  vertical-align: middle;
}

.edit-input {
  background: transparent;
  border: 1px solid transparent;
  color: #ccc;
  padding: 2px 5px;
  width: 100%;
  box-sizing: border-box;
  font-size: 13px;
  height: 26px;
}

.edit-input:hover, .edit-input:focus {
  background: #1e1e1e;
  border-color: #555;
  color: white;
  outline: none;
}

.edit-input.pwd {
  font-family: monospace;
}

.data-grid tr:hover {
  background-color: #3e3e42;
}

.status-badge {
  padding: 1px 6px;
  border-radius: 10px;
  font-size: 11px;
}

.status-ok { background-color: #2d5a27; }
.status-warn { background-color: #8b0000; }

.cooldown-text {
  font-size: 10px;
  opacity: 0.85;
  margin-left: 4px;
  font-family: monospace;
}

.link-cell {
  color: #007ACC;
  cursor: pointer;
}

.link-cell:hover { text-decoration: underline; }

.row-actions {
  display: flex;
  gap: 5px;
  align-items: center;
  justify-content: center;
  height: 34px; /* 与 td 高度一致 */
}

.btn-sm {
  padding: 0 8px;
  height: 24px;
  font-size: 11px;
  border: none;
  border-radius: 2px;
  cursor: pointer;
  color: white;
  display: flex;
  align-items: center;
  justify-content: center;
}

.btn-sm.login { background-color: #007ACC; }
.btn-sm.delete { background-color: #444; }
.btn-sm:hover { filter: brightness(1.2); }

/* 状态栏 */
.status-bar {
  background-color: #007ACC;
  padding: 2px 10px;
  display: flex;
  justify-content: space-between;
  font-size: 12px;
}

/* 自定义滚动条 */
.table-container::-webkit-scrollbar {
  width: 8px;
}

.table-container::-webkit-scrollbar-thumb {
  background: #555;
}

.table-container::-webkit-scrollbar-track {
  background: #2d2d30;
}
</style>

