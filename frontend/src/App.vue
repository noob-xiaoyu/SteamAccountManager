<script setup>
import { ref, onMounted, onUnmounted, computed, watch } from 'vue';

const accounts = ref([]);
const apiKey = ref(localStorage.getItem('steam_api_key') || '');
const showApiKey = ref(false);
const visiblePasswords = ref({}); // 记录每个账号密码的可见性

const togglePassword = (id) => {
  visiblePasswords.value[id] = !visiblePasswords.value[id];
};

const showBulkAdd = ref(false);
const showEditPanel = ref(false);
const showSettings = ref(false);
const showAbout = ref(false);
const settingsMenuPos = ref({ x: 0, y: 0 });
const isDarkTheme = ref(localStorage.getItem('theme') !== 'light');

const toggleSettingsMenu = (event) => {
  const btn = event.currentTarget;
  const rect = btn.getBoundingClientRect();
  settingsMenuPos.value = {
    x: rect.left - 120,
    y: rect.bottom
  };
  showSettings.value = !showSettings.value;
};

const closeSettingsMenu = () => {
  showSettings.value = false;
};

const handleSettingsAction = (action) => {
  closeSettingsMenu();
  switch (action) {
    case 'theme':
      isDarkTheme.value = !isDarkTheme.value;
      localStorage.setItem('theme', isDarkTheme.value ? 'dark' : 'light');
      statusText.value = isDarkTheme.value ? '已切换到深色主题' : '已切换到浅色主题';
      saveConfig();
      break;
    case 'about':
      closeSettingsMenu();
      showAbout.value = true;
      break;
  }
};

const toggleSidebar = () => {
  isSidebarCollapsed.value = !isSidebarCollapsed.value;
  saveConfig();
};

const selectedUsernames = ref(new Set());
const editingAccount = ref(null);
const cooldownDaysInput = ref(0); // 快捷设置冷却天数
const bulkText = ref('');
const isBusy = ref(false);
const statusText = ref('就绪');
const currentTab = ref('all'); // 当前显示的页签: 'all' 或 'main'
const isSidebarCollapsed = ref(false); // 侧边栏折叠状态
const navMenuRef = ref(null);
let cooldownTimer = null; // 定时检查冷却
let lastSelectedUsername = null;
let dragTimeout = null;

// 窗口缩放相关变量
const resizeBorderSize = 8; // 边缘检测区域大小（像素）
const mousePosition = ref({ x: 0, y: 0 });
const isOnEdge = ref(false);
const edgeType = ref(''); // 'left', 'right', 'top', 'bottom', 'top-left', 'top-right', 'bottom-left', 'bottom-right'
const resizeCursor = ref('default');

const navIndicatorStyle = computed(() => {
  const indexMap = { 'main': 0, 'perfect': 1, '5e': 2, 'all': 3 };
  const index = indexMap[currentTab.value] ?? 0;
  return {
    transform: `translateY(${index * 40}px)`
  };
});

const navigateTo = (tab) => {
  currentTab.value = tab;
};

const contextMenu = ref({ show: false, x: 0, y: 0, account: null });

// 根据当前页签过滤账号
const filteredAccounts = computed(() => {
  if (currentTab.value === 'main') return accounts.value.filter(a => a.category === 'main');
  if (currentTab.value === 'perfect') return accounts.value.filter(a => a.category === 'perfect');
  if (currentTab.value === '5e') return accounts.value.filter(a => a.category === '5e');
  return accounts.value.filter(a => !a.category || a.category === 'normal');
});

const hasSelection = computed(() => selectedUsernames.value.size > 0);

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
      } else if (event.data.type === 'setConfig') {
        const config = event.data.config || {};
        if (config.sidebarCollapsed !== undefined) {
          isSidebarCollapsed.value = config.sidebarCollapsed;
        }
        if (config.theme !== undefined) {
          isDarkTheme.value = config.theme === 'dark';
          localStorage.setItem('theme', config.theme);
        }
      }
    });
    // 向 C++ 请求账号数据和配置
    window.chrome.webview.postMessage({ type: 'getAccounts' });
    window.chrome.webview.postMessage({ type: 'getConfig' });
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

const saveConfig = () => {
  if (window.chrome && window.chrome.webview) {
    const config = {
      sidebarCollapsed: isSidebarCollapsed.value,
      theme: isDarkTheme.value ? 'dark' : 'light'
    };
    window.chrome.webview.postMessage({
      type: 'saveConfig',
      config: config
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

    let username = '';
    let password = '';
    let alias = '';

    // 检测分隔符：优先匹配 -- 分隔的格式
    if (trimmedLine.includes('----')) {
      const parts = trimmedLine.split('----');
      username = parts[0].trim();
      password = parts[1] ? parts[1].trim() : '';
      alias = username;
    } else if (trimmedLine.includes('---')) {
      const parts = trimmedLine.split('---');
      username = parts[0].trim();
      password = parts[1] ? parts[1].trim() : '';
      alias = username;
    } else if (trimmedLine.includes('--')) {
      const parts = trimmedLine.split('--');
      username = parts[0].trim();
      password = parts[1] ? parts[1].trim() : '';
      alias = username;
    } else if (trimmedLine.includes('-')) {
      const parts = trimmedLine.split('-');
      username = parts[0].trim();
      password = parts[1] ? parts[1].trim() : '';
      alias = username;
    } else if (trimmedLine.includes('，')) {
      const parts = trimmedLine.split('，');
      username = parts[0].trim();
      password = parts[1] ? parts[1].trim() : '';
      alias = username;
    } else if (trimmedLine.includes(',')) {
      const parts = trimmedLine.split(',');
      username = parts[0].trim();
      password = parts[1] ? parts[1].trim() : '';
      alias = username;
    } else {
      username = trimmedLine;
      alias = trimmedLine;
    }

    if (username) {
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
    if (e.detail === 2) return;
    
    // 如果鼠标在边缘区域（包括标题栏内的左右边缘和顶部区域），不处理拖动（让缩放处理）
    if (isOnEdge.value) {
      return;
    }
    
    // 阻止事件冒泡，避免触发外层的handleResizeStart
    e.stopPropagation();
    
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage({ type: 'windowControl', action: 'startDrag' });
    }
  }
};

// 鼠标移动处理窗口缩放
const handleMouseMove = (e) => {
  mousePosition.value = { x: e.clientX, y: e.clientY };
  
  const width = window.innerWidth;
  const height = window.innerHeight;
  const border = resizeBorderSize;
  const titleBarHeight = 32; // 标题栏高度
  const titleBarTopAreaHeight = 4; // 标题栏顶部用于缩放的区域高度
  
  // 检查鼠标位置区域
  const isInTitleBarTopArea = e.clientY < titleBarTopAreaHeight; // 标题栏顶部4px区域
  const isInTitleBarBody = e.clientY >= titleBarTopAreaHeight && e.clientY < titleBarHeight; // 标题栏主体区域
  const isInTitleBar = isInTitleBarTopArea || isInTitleBarBody; // 整个标题栏区域
  
  // 检查鼠标是否在边缘区域
  const left = e.clientX < border;
  const right = e.clientX > width - border;
  const top = e.clientY < border;
  const bottom = e.clientY > height - border;
  
  // 确定边缘类型和光标样式
  let newEdgeType = '';
  let newCursor = 'default';
  
  // 如果在标题栏顶部区域（4px），进行完整的边缘检测（包括顶部缩放）
  if (isInTitleBarTopArea) {
    if (top && left) {
      newEdgeType = 'top-left';
      newCursor = 'nwse-resize';
    } else if (top && right) {
      newEdgeType = 'top-right';
      newCursor = 'nesw-resize';
    } else if (bottom && left) {
      newEdgeType = 'bottom-left';
      newCursor = 'nesw-resize';
    } else if (bottom && right) {
      newEdgeType = 'bottom-right';
      newCursor = 'nwse-resize';
    } else if (left) {
      newEdgeType = 'left';
      newCursor = 'ew-resize';
    } else if (right) {
      newEdgeType = 'right';
      newCursor = 'ew-resize';
    } else if (top) {
      newEdgeType = 'top';
      newCursor = 'ns-resize';
    } else if (bottom) {
      newEdgeType = 'bottom';
      newCursor = 'ns-resize';
    }
  }
  // 如果在标题栏主体区域（28px），忽略顶部和顶部角落的边缘检测（允许拖动）
  else if (isInTitleBarBody) {
    if (bottom && left) {
      newEdgeType = 'bottom-left';
      newCursor = 'nesw-resize';
    } else if (bottom && right) {
      newEdgeType = 'bottom-right';
      newCursor = 'nwse-resize';
    } else if (left) {
      newEdgeType = 'left';
      newCursor = 'ew-resize';
    } else if (right) {
      newEdgeType = 'right';
      newCursor = 'ew-resize';
    } else if (bottom) {
      newEdgeType = 'bottom';
      newCursor = 'ns-resize';
    }
    // 标题栏主体区域内的顶部和顶部角落不视为边缘，保持默认光标
  }
  // 如果不在标题栏内，正常检测所有边缘
  else {
    if (top && left) {
      newEdgeType = 'top-left';
      newCursor = 'nwse-resize';
    } else if (top && right) {
      newEdgeType = 'top-right';
      newCursor = 'nesw-resize';
    } else if (bottom && left) {
      newEdgeType = 'bottom-left';
      newCursor = 'nesw-resize';
    } else if (bottom && right) {
      newEdgeType = 'bottom-right';
      newCursor = 'nwse-resize';
    } else if (left) {
      newEdgeType = 'left';
      newCursor = 'ew-resize';
    } else if (right) {
      newEdgeType = 'right';
      newCursor = 'ew-resize';
    } else if (top) {
      newEdgeType = 'top';
      newCursor = 'ns-resize';
    } else if (bottom) {
      newEdgeType = 'bottom';
      newCursor = 'ns-resize';
    }
  }
  
  isOnEdge.value = newEdgeType !== '';
  edgeType.value = newEdgeType;
  resizeCursor.value = newCursor;
};

const handleMouseLeave = () => {
  isOnEdge.value = false;
  edgeType.value = '';
  resizeCursor.value = 'default';
};

const handleResizeStart = (e) => {
  if (e.button !== 0) return; // 只处理左键
  if (!isOnEdge.value) return; // 不在边缘时不处理
  
  // 如果点击的是标题栏区域，检查是否在边缘区域（允许缩放）
  if (e.target.closest('.custom-titlebar')) {
    // 如果在标题栏内，但不是在边缘区域（left/right/top/top-left/top-right），则让handleDragStart处理拖动
    if (edgeType.value !== 'left' && edgeType.value !== 'right' && 
        edgeType.value !== 'top' && edgeType.value !== 'top-left' && edgeType.value !== 'top-right') {
      return;
    }
    // 如果是在标题栏边缘区域，则继续处理缩放
  }
  
  e.preventDefault();
  e.stopPropagation();
  
  if (window.chrome && window.chrome.webview) {
    let action = '';
    switch (edgeType.value) {
      case 'left': action = 'startResizeLeft'; break;
      case 'right': action = 'startResizeRight'; break;
      case 'top': action = 'startResizeTop'; break;
      case 'bottom': action = 'startResizeBottom'; break;
      case 'top-left': action = 'startResizeTopLeft'; break;
      case 'top-right': action = 'startResizeTopRight'; break;
      case 'bottom-left': action = 'startResizeBottomLeft'; break;
      case 'bottom-right': action = 'startResizeBottomRight'; break;
      default: return;
    }
    
    window.chrome.webview.postMessage({ type: 'windowControl', action });
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
  if (!e || !e.target.closest('.settings-menu')) {
    showSettings.value = false;
  }
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
  <div class="app-container" :class="{ 'is-busy': isBusy, 'light-theme': !isDarkTheme }" 
       @mousemove="handleMouseMove"
       @mouseleave="handleMouseLeave"
       @mousedown="handleResizeStart"
       :style="{ cursor: resizeCursor }">
    <!-- 自定义标题栏 -->
    <div class="custom-titlebar" 
         @dblclick="handleWindowControl('maximize')"
         @mousedown="handleDragStart">
      <div class="title-content">
        <span class="app-title">Steam Account Manager</span>
      </div>
      <div class="window-controls">
        <button class="win-btn settings-btn" @click.stop="toggleSettingsMenu" @dblclick.stop title="设置">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="3"/>
            <path d="M19.4 15a1.65 1.65 0 0 0 .33 1.82l.06.06a2 2 0 0 1 0 2.83 2 2 0 0 1-2.83 0l-.06-.06a1.65 1.65 0 0 0-1.82-.33 1.65 1.65 0 0 0-1 1.51V21a2 2 0 0 1-2 2 2 2 0 0 1-2-2v-.09A1.65 1.65 0 0 0 9 19.4a1.65 1.65 0 0 0-1.82.33l-.06.06a2 2 0 0 1-2.83 0 2 2 0 0 1 0-2.83l.06-.06a1.65 1.65 0 0 0 .33-1.82 1.65 1.65 0 0 0-1.51-1H3a2 2 0 0 1-2-2 2 2 0 0 1 2-2h.09A1.65 1.65 0 0 0 4.6 9a1.65 1.65 0 0 0-.33-1.82l-.06-.06a2 2 0 0 1 0-2.83 2 2 0 0 1 2.83 0l.06.06a1.65 1.65 0 0 0 1.82.33H9a1.65 1.65 0 0 0 1-1.51V3a2 2 0 0 1 2-2 2 2 0 0 1 2 2v.09a1.65 1.65 0 0 0 1 1.51 1.65 1.65 0 0 0 1.82-.33l.06-.06a2 2 0 0 1 2.83 0 2 2 0 0 1 0 2.83l-.06.06a1.65 1.65 0 0 0-.33 1.82V9a1.65 1.65 0 0 0 1.51 1H21a2 2 0 0 1 2 2 2 2 0 0 1-2 2h-.09a1.65 1.65 0 0 0-1.51 1z"/>
          </svg>
        </button>
        <button class="win-btn" @click="handleWindowControl('minimize')" title="最小化">
          <svg viewBox="0 0 10 10"><path d="M0 5h10v1H0z" fill="currentColor"/></svg>
        </button>
        <button class="win-btn" @click="handleWindowControl('maximize')" title="最大化/还原">
          <svg viewBox="0 0 10 10"><path d="M0 0v10h10V0H0zm1 1h8v8H1V1z" fill="currentColor"/></svg>
        </button>
        <button class="win-btn close" @click="handleWindowControl('close')" title="关闭">
          <svg viewBox="0 0 10 10"><path d="M0 0l10 10M10 0L0 10" stroke="currentColor" stroke-width="1.2"/></svg>
        </button>
      </div>
    </div>

    <div class="layout-body">
      <!-- 左侧导航侧边栏 -->
      <aside class="sidebar" :class="{ 'collapsed': isSidebarCollapsed }">
        <div class="sidebar-header">
          <span v-if="!isSidebarCollapsed">SAM</span>
          <button @click="toggleSidebar" class="toggle-sidebar-btn">
            {{ isSidebarCollapsed ? '→' : '←' }}
          </button>
        </div>
        <nav class="nav-menu" ref="navMenuRef">
          <div class="nav-indicator" :style="navIndicatorStyle"></div>
          <button @click="navigateTo('main')" :class="['nav-item', { active: currentTab === 'main' }]" :title="isSidebarCollapsed ? '主账号' : ''">
            <span class="icon">⭐</span>
            <span class="label">主账号</span>
          </button>
          <button @click="navigateTo('perfect')" :class="['nav-item', { active: currentTab === 'perfect' }]" :title="isSidebarCollapsed ? '完美账号' : ''">
            <span class="icon"><img src="./assets/perfect.png" style="width: 16px; height: 16px; object-fit: contain; vertical-align: middle;" /></span>
            <span class="label">完美账号</span>
          </button>
          <button @click="navigateTo('5e')" :class="['nav-item', { active: currentTab === '5e' }]" :title="isSidebarCollapsed ? '5E账号' : ''">
            <span class="icon"><img src="./assets/5E.png" style="width: 16px; height: 16px; object-fit: contain; vertical-align: middle;" /></span>
            <span class="label">5E账号</span>
          </button>
          <button @click="navigateTo('all')" :class="['nav-item', { active: currentTab === 'all' }]" :title="isSidebarCollapsed ? '普通账号' : ''">
            <span class="icon">📋</span>
            <span class="label">普通账号</span>
          </button>
        </nav>
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
          <button @click="showBulkAdd = !showBulkAdd; showEditPanel = false" class="btn btn-blue">➕批量添加➕</button>
          <button @click="handleEditSelected" class="btn" :disabled="!hasSelection">✏️更改选中✏️</button>
          <button @click="handleBulkDelete" class="btn btn-red" :disabled="!hasSelection">🗑️删除选中🗑️</button>
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
                             :type="visiblePasswords[account.id] ? 'text' : 'password'"
                             autocomplete="new-password" spellcheck="false"
                             class="edit-input pwd" placeholder="无密码" />
                      <button class="toggle-btn" @click="togglePassword(account.id)">
                        <svg v-if="visiblePasswords[account.id]" width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
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
                  <td colspan="6" style="text-align: center; padding: 40px; color: var(--text-muted);">
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
          <p style="margin: 20px 0; font-size: 14px; color: var(--text-secondary);">
            <template v-if="accountsToDelete.length === 1">
              确定要删除账号 <br/><br/>
              <strong style="color: var(--text-primary); font-size: 18px;">{{ accountsToDelete[0].username }}</strong> <br/><br/>
              吗？
            </template>
            <template v-else>
              确定要批量删除 <br/><br/>
              <strong style="color: var(--text-primary); font-size: 18px;">已选中的 {{ accountsToDelete.length }} 个账号</strong> <br/><br/>
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
      <span class="right">v1.0.0</span>
    </footer>

    <!-- 设置菜单 -->
    <Transition name="context-menu">
      <div v-if="showSettings"
           class="context-menu settings-menu"
           :style="{ left: settingsMenuPos.x + 'px', top: settingsMenuPos.y + 'px' }"
           @click.stop>
        <div class="context-menu-item" @click="handleSettingsAction('theme')">
          {{ isDarkTheme ? '☀️ 浅色主题' : '🌙 深色主题' }}
        </div>
        <div class="context-menu-divider"></div>
        <div class="context-menu-item" @click="handleSettingsAction('about')">ℹ️ 关于</div>
      </div>
    </Transition>

    <!-- 弹窗：关于 -->
    <Transition name="modal-fade">
      <div v-if="showAbout" class="modal-overlay" @click.self="showAbout = false">
        <div class="modal-dialog about-panel">
          <h3 class="modal-title">ℹ️ 关于</h3>
          <div class="about-content">
            <div class="about-logo">📋</div>
            <div class="about-name">Steam Account Manager</div>
            <div class="about-version">版本 1.2.0</div>
            <div class="about-divider"></div>
            <div class="about-author">开发者: noob-xiaoyu</div>
            <div class="about-desc">一个简洁高效的 Steam 账号管理工具</div>
            <div class="about-link" @click="openExternalLink('https://github.com/noob-xiaoyu/SteamAccountManager')">
              🌐 GitHub: noob-xiaoyu/SteamAccountManager
            </div>
          </div>
          <div class="about-actions">
            <button @click="showAbout = false" class="btn">关闭</button>
          </div>
        </div>
      </div>
    </Transition>
  </div>
</template>

<style scoped>
/* 表格行选择样式 */
.is-selected {
  background-color: var(--selected-bg) !important;
  outline: 1px solid var(--accent-primary);
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
  background-color: var(--bg-secondary);
  border: 1px solid var(--border-color);
  border-radius: 8px;
  padding: 20px;
  width: 500px;
  max-width: 90vw;
  box-shadow: var(--shadow);
}

.modal-title {
  margin-top: 0;
  margin-bottom: 20px;
  font-size: 16px;
  font-weight: 600;
  color: var(--accent-primary);
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
  color: var(--text-secondary);
  margin-bottom: 5px;
}

.input-group input, .input-group select {
  background-color: var(--bg-primary);
  border: 1px solid var(--border-color);
  color: var(--text-primary);
  padding: 8px;
  border-radius: 4px;
}

.input-group input:focus {
  border-color: var(--accent-primary);
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
  background-color: var(--bg-primary);
  color: var(--text-primary);
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
  overflow: hidden;
  box-sizing: border-box;
}

/* 自定义标题栏 */
.custom-titlebar {
  height: 32px;
  background-color: var(--bg-secondary);
  display: flex;
  justify-content: space-between;
  align-items: center;
  user-select: none;
  z-index: 9999;
  border-bottom: 1px solid var(--border-color);
  flex-shrink: 0;
}

.title-content {
  display: flex;
  align-items: center;
  padding-left: 12px;
  color: var(--text-muted);
  font-size: 12px;
  pointer-events: none;
}

.window-controls {
  display: flex;
  height: 100%;
}

.settings-btn {
  margin-left: 10px;
}

.settings-btn svg {
  width: 16px;
  height: 16px;
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
  background-color: var(--hover-bg);
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
  background-color: var(--bg-secondary);
  border-right: 1px solid var(--border-color);
  display: flex;
  flex-direction: column;
  flex-shrink: 0;
  transition: width 0.3s cubic-bezier(0.4, 0, 0.2, 1);
  overflow-x: hidden;
}

.sidebar.collapsed {
  width: 50px;
}

.sidebar-header {
  padding: 0 15px;
  font-weight: bold;
  font-size: 18px;
  color: var(--accent-primary);
  display: flex;
  justify-content: space-between;
  align-items: center;
  border-bottom: 1px solid var(--border-color);
  height: 44.5px;
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
  color: var(--text-muted);
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
  background-color: var(--hover-bg);
  color: var(--text-primary);
}

.nav-menu {
  flex: 1;
  padding: 10px 0;
  position: relative;
}

.nav-indicator {
  position: absolute;
  top: 10px;
  left: 0;
  width: 3px;
  height: 40px;
  background-color: var(--accent-primary);
  border-radius: 0 2px 2px 0;
  transition: transform 0.2s ease;
  z-index: 1;
}

.nav-item {
  width: 160px;
  height: 40px;
  display: flex;
  align-items: center;
  background: transparent;
  border: none;
  color: var(--text-secondary);
  cursor: pointer;
  transition: background-color 0.2s, color 0.2s;
  font-size: 14px;
  padding: 0;
  text-align: left;
}

.nav-item:hover {
  background-color: var(--hover-bg);
  color: var(--text-primary);
}

.nav-item.active {
  background-color: var(--selected-bg);
  color: var(--text-primary);
}

.nav-item .icon {
  font-size: 16px;
  width: 50px;
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
  opacity: 0;
}

.sidebar-footer {
  padding: 10px;
  text-align: center;
  font-size: 10px;
  color: var(--text-muted);
  border-top: 1px solid var(--border-color);
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
  color: var(--text-secondary);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  max-width: 200px;
}

.spacer {
  flex: 1;
}

/* 工具栏 */
.toolbar {
  display: flex;
  gap: 10px;
  padding: 10px 15px;
  background-color: var(--bg-secondary);
  border-bottom: 1px solid var(--border-color);
}

/* 自定义标题栏 */
.custom-titlebar {
  height: 32px;
  background-color: var(--bg-secondary);
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
  color: var(--text-muted);
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
  color: var(--text-primary);
  cursor: pointer;
  transition: background-color 0.2s;
}

.win-btn:hover {
  background-color: var(--hover-bg);
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
  background-color: var(--bg-tertiary);
  color: var(--text-primary);
  border: 1px solid var(--border-color);
  padding: 0 12px;
  cursor: pointer;
  font-size: 13px;
  border-radius: 3px;
  height: 24px;
  display: flex;
  align-items: center;
  white-space: nowrap;
  flex-shrink: 0;
}

.btn:hover {
  background-color: var(--hover-bg);
}

.btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
  pointer-events: none;
}

.btn.primary {
  border-color: var(--accent-primary);
}

.btn.success {
  background-color: var(--success-bg);
}

.btn.btn-blue {
  border-color: var(--accent-primary) !important;
  color: var(--accent-primary) !important;
}

.btn.btn-red {
  border-color: #ff5252 !important;
  color: #ff5252 !important;
}

/* API Key 区 */
.api-key-area {
  height: 34px;
  padding: 0 18px;
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 13px;
  background-color: var(--bg-secondary);
  border-bottom: 1px solid var(--border-color);
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
  background-color: var(--bg-tertiary);
  border: 1px solid var(--border-color);
  color: var(--text-primary);
  padding: 0 30px 0 8px;
  width: 100%;
  height: 24px;
}

.toggle-btn {
  position: absolute;
  right: 4px;
  background: none;
  border: none;
  color: var(--text-muted);
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 2px;
  transition: color 0.2s;
}

.toggle-btn:hover {
  color: var(--text-secondary);
}

.table-pwd-wrapper {
  width: 130px;
}

.table-pwd-wrapper input {
  padding-right: 25px !important;
}

.api-key-area a {
  color: var(--accent-primary);
  text-decoration: none;
}

/* 批量面板 */
.bulk-panel {
  padding: 10px;
  background-color: var(--bg-secondary);
  border-bottom: 1px solid var(--border-color);
}

.bulk-panel textarea {
  width: 100%;
  height: 80px;
  background-color: var(--bg-primary);
  color: var(--text-primary);
  border: 1px solid var(--border-color);
  padding: 5px;
  resize: vertical;
  margin-bottom: 5px;
}

.bulk-panel textarea::-webkit-scrollbar {
  width: 8px;
}

.bulk-panel textarea::-webkit-scrollbar-thumb {
  background: var(--border-color);
  border-radius: 4px;
}

.bulk-panel textarea::-webkit-scrollbar-track {
  background: var(--bg-primary);
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
  background-color: var(--bg-secondary);
  border: 1px solid var(--border-color);
  border-radius: 6px;
  padding: 5px 0;
  min-width: 160px;
  box-shadow: var(--shadow);
  z-index: 10000;
}

.context-menu-item {
  padding: 8px;
  cursor: pointer;
  font-size: 13px;
  color: var(--text-secondary);
  display: flex;
  align-items: center;
  gap: 8px;
}

.context-menu-item:hover {
  padding: 8px 8px;
  background-color: var(--accent-dark);
  color: var(--text-primary);
}

.context-menu-item.danger:hover {
  background-color: #8b0000;
}

.context-menu-divider {
  height: 1px;
  background-color: var(--border-color);
  margin: 5px 0;
}

.settings-menu {
  min-width: 180px;
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
  overflow: auto;
}

.data-grid {
  width: 100%;
  border-collapse: collapse;
  font-size: 13px;
  table-layout: fixed;
}

.data-grid th, .data-grid td {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  padding: 0 8px;
}

.data-grid th {
  background-color: var(--bg-tertiary);
  text-align: left;
  height: 34px;
  border: 1px solid var(--border-color);
  position: sticky;
  top: 0;
}

.data-grid td {
  height: 34px;
  border-bottom: 1px solid var(--border-color);
  vertical-align: middle;
}

.edit-input {
  background: transparent;
  border: 1px solid transparent;
  color: var(--text-secondary);
  padding: 2px 5px;
  width: 100%;
  box-sizing: border-box;
  font-size: 13px;
  height: 26px;
}

.edit-input:hover, .edit-input:focus {
  background: var(--bg-primary);
  border-color: var(--border-color);
  color: var(--text-primary);
  outline: none;
}

.edit-input.pwd {
  font-family: monospace;
}

.data-grid tr:hover {
  background-color: var(--hover-bg);
}

.status-badge {
  padding: 1px 6px;
  border-radius: 10px;
  font-size: 11px;
}

.status-ok { background-color: var(--success-bg); color: var(--text-primary); }
.status-warn { background-color: var(--danger-bg); }

.cooldown-text {
  font-size: 10px;
  opacity: 0.85;
  margin-left: 4px;
  font-family: monospace;
}

.link-cell {
  color: var(--accent-primary);
  cursor: pointer;
}

.link-cell:hover { text-decoration: underline; }

.row-actions {
  display: flex;
  gap: 5px;
  align-items: center;
  justify-content: center;
  height: 34px;
}

.btn-sm {
  padding: 0 8px;
  height: 24px;
  font-size: 11px;
  border: none;
  border-radius: 2px;
  cursor: pointer;
  color: var(--text-primary);
  display: flex;
  align-items: center;
  justify-content: center;
}

.btn-sm.login { background-color: var(--accent-primary); }
.btn-sm.delete { background-color: var(--bg-tertiary); }
.btn-sm:hover { filter: brightness(1.2); }

/* 状态栏 */
.status-bar {
  background-color: var(--accent-primary);
  color: var(--text-primary);
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
  background: var(--border-color);
}

.table-container::-webkit-scrollbar-track {
  background: var(--bg-primary);
}

/* 浅色主题 */
.light-theme {
  --bg-primary: #ffffff;
  --bg-secondary: #f3f3f3;
  --bg-tertiary: #e8e8e8;
  --accent-primary: #0078d4;
  --accent-secondary: #106ebe;
  --accent-dark: #005a9e;
  --text-primary: #1e1e1e;
  --text-secondary: #333333;
  --text-muted: #666666;
  --border-color: #d4d4d4;
  --hover-bg: #e1e1e1;
  --selected-bg: #cce8f4;
  --success-bg: #a8d8a8;
  --danger-bg: #f0a8a8;
  --shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
}

.light-theme {
  background-color: var(--bg-primary) !important;
  color: var(--text-primary) !important;
}

.light-theme .custom-titlebar {
  background-color: var(--bg-secondary) !important;
  border-bottom-color: var(--border-color) !important;
}

.light-theme .title-content {
  color: var(--text-muted) !important;
}

.light-theme .app-title {
  color: var(--text-primary) !important;
}

.light-theme .sidebar {
  background-color: var(--bg-secondary) !important;
  border-right-color: var(--border-color) !important;
}

.light-theme .sidebar-header {
  color: var(--accent-primary) !important;
  border-bottom-color: var(--border-color) !important;
}

.light-theme .nav-item {
  color: var(--text-secondary) !important;
}

.light-theme .nav-item:hover {
  background-color: var(--hover-bg) !important;
  color: var(--text-primary) !important;
}

.light-theme .nav-item.active {
  background-color: var(--selected-bg) !important;
  color: var(--text-primary) !important;
}

.light-theme .toolbar {
  background-color: var(--bg-secondary) !important;
  border-bottom-color: var(--border-color) !important;
}

.light-theme .view-title {
  color: var(--text-primary) !important;
}

.light-theme .data-grid th {
  background-color: var(--bg-tertiary) !important;
  color: var(--text-primary) !important;
  border-color: var(--border-color) !important;
}

.light-theme .data-grid td {
  background-color: var(--bg-primary) !important;
  color: var(--text-primary) !important;
  border-bottom-color: var(--border-color) !important;
}

.light-theme .data-grid tr:hover td {
  background-color: var(--hover-bg) !important;
}

.light-theme .edit-input {
  background: transparent !important;
  color: var(--text-primary) !important;
}

.light-theme .edit-input:hover,
.light-theme .edit-input:focus {
  background: var(--bg-primary) !important;
  border-color: var(--accent-primary) !important;
  color: var(--text-primary) !important;
}

.light-theme .status-bar {
  background-color: var(--accent-primary) !important;
  color: var(--bg-primary) !important;
}

.light-theme .win-btn {
  color: var(--text-primary) !important;
}

.light-theme .win-btn svg {
  stroke: var(--text-primary) !important;
}

.light-theme .modal-overlay {
  background-color: rgba(0, 0, 0, 0.4) !important;
}

.light-theme .modal-dialog {
  background-color: var(--bg-primary) !important;
  border-color: var(--border-color) !important;
  color: var(--text-primary) !important;
}

.light-theme .modal-title {
  color: var(--accent-primary) !important;
}

.light-theme .input-group label {
  color: var(--text-secondary) !important;
}

.light-theme .input-group input,
.light-theme .input-group select {
  background-color: var(--bg-primary) !important;
  border-color: var(--border-color) !important;
  color: var(--text-primary) !important;
}

.light-theme .input-group input:focus {
  border-color: var(--accent-primary) !important;
}

.light-theme .context-menu {
  background-color: var(--bg-primary) !important;
  border-color: var(--border-color) !important;
  color: var(--text-primary) !important;
}

.light-theme .context-menu-item {
  color: var(--text-primary) !important;
}

.light-theme .context-menu-item:hover {
  background-color: var(--accent-secondary) !important;
  color: var(--bg-primary) !important;
}

.light-theme .btn {
  background-color: var(--bg-tertiary) !important;
  color: var(--text-primary) !important;
  border-color: var(--border-color) !important;
}

.light-theme .btn:hover {
  background-color: var(--hover-bg) !important;
}

.light-theme .btn.btn-blue {
  border-color: #0078d4 !important;
  color: #0078d4 !important;
}

.light-theme .btn.btn-red {
  border-color: #d32f2f !important;
  color: #d32f2f !important;
}

.light-theme .bulk-panel {
  background-color: var(--bg-secondary) !important;
  border-bottom-color: var(--border-color) !important;
}

.light-theme .bulk-panel textarea {
  background-color: var(--bg-primary) !important;
  color: var(--text-primary) !important;
  border-color: var(--border-color) !important;
}

.light-theme .settings-menu {
  background-color: var(--bg-primary) !important;
  border-color: var(--border-color) !important;
}

.light-theme .is-selected {
  background-color: var(--selected-bg) !important;
  outline-color: var(--accent-primary) !important;
}

.about-panel {
  width: 360px;
  text-align: center;
}

.about-content {
  padding: 20px 0;
}

.about-logo {
  font-size: 48px;
  margin-bottom: 15px;
}

.about-name {
  font-size: 20px;
  font-weight: 600;
  color: var(--accent-primary);
  margin-bottom: 8px;
}

.about-version {
  font-size: 13px;
  color: var(--text-muted);
  margin-bottom: 15px;
}

.about-divider {
  height: 1px;
  background-color: var(--border-color);
  margin: 15px 0;
}

.about-author {
  font-size: 14px;
  color: var(--text-secondary);
  margin-bottom: 8px;
}

.about-desc {
  font-size: 13px;
  color: var(--text-muted);
  margin-bottom: 15px;
}

.about-link {
  font-size: 13px;
  color: var(--accent-primary);
  cursor: pointer;
  padding: 8px;
  border-radius: 4px;
  transition: background-color 0.2s;
}

.about-link:hover {
  background-color: var(--hover-bg);
}

.about-actions {
  margin-top: 20px;
}
</style>

