// ===========================
// Initialize Mermaid
// ===========================
mermaid.initialize({
    startOnLoad: true,
    theme: 'default',
    themeVariables: {
        primaryColor: '#4f46e5',
        primaryTextColor: '#fff',
        primaryBorderColor: '#4338ca',
        lineColor: '#6366f1',
        secondaryColor: '#06b6d4',
        tertiaryColor: '#f59e0b'
    },
    flowchart: {
        useMaxWidth: true,
        htmlLabels: true,
        curve: 'basis'
    },
    sequence: {
        diagramMarginX: 50,
        diagramMarginY: 10,
        actorMargin: 50,
        width: 150,
        height: 65,
        boxMargin: 10,
        boxTextMargin: 5,
        noteMargin: 10,
        messageMargin: 35
    }
});

// ===========================
// Smooth Scrolling
// ===========================
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function (e) {
        const href = this.getAttribute('href');
        if (href !== '#' && href.startsWith('#')) {
            e.preventDefault();
            const target = document.querySelector(href);
            if (target) {
                const navHeight = document.querySelector('.navbar').offsetHeight;
                const targetPosition = target.offsetTop - navHeight;
                window.scrollTo({
                    top: targetPosition,
                    behavior: 'smooth'
                });
            }
        }
    });
});

// ===========================
// Scroll to Top Button
// ===========================
const scrollToTopBtn = document.querySelector('.scroll-to-top');

window.addEventListener('scroll', () => {
    if (window.pageYOffset > 300) {
        scrollToTopBtn.classList.add('visible');
    } else {
        scrollToTopBtn.classList.remove('visible');
    }
});

scrollToTopBtn.addEventListener('click', () => {
    window.scrollTo({
        top: 0,
        behavior: 'smooth'
    });
});

// ===========================
// Tab Functionality
// ===========================
const tabButtons = document.querySelectorAll('.tab-btn');
const tabContents = document.querySelectorAll('.tab-content');

tabButtons.forEach(button => {
    button.addEventListener('click', () => {
        const tabName = button.getAttribute('data-tab');
        
        // Remove active class from all buttons and contents
        tabButtons.forEach(btn => btn.classList.remove('active'));
        tabContents.forEach(content => content.classList.remove('active'));
        
        // Add active class to clicked button and corresponding content
        button.classList.add('active');
        const activeContent = document.getElementById(tabName);
        if (activeContent) {
            activeContent.classList.add('active');
        }
    });
});

// ===========================
// Copy Code Functionality
// ===========================
document.querySelectorAll('.copy-btn').forEach(button => {
    button.addEventListener('click', async () => {
        const codeId = button.getAttribute('data-code');
        const codeElement = document.getElementById(codeId);
        
        if (codeElement) {
            try {
                await navigator.clipboard.writeText(codeElement.textContent);
                
                // Show feedback
                const originalText = button.textContent;
                button.textContent = 'Copied!';
                button.style.background = 'rgba(16, 185, 129, 0.2)';
                
                setTimeout(() => {
                    button.textContent = originalText;
                    button.style.background = '';
                }, 2000);
            } catch (err) {
                console.error('Failed to copy:', err);
                button.textContent = 'Failed';
                setTimeout(() => {
                    button.textContent = 'Copy';
                }, 2000);
            }
        }
    });
});

// ===========================
// Mobile Menu Toggle
// ===========================
const mobileMenuToggle = document.querySelector('.mobile-menu-toggle');
const navMenu = document.querySelector('.nav-menu');

if (mobileMenuToggle && navMenu) {
    mobileMenuToggle.addEventListener('click', () => {
        navMenu.classList.toggle('active');
        mobileMenuToggle.classList.toggle('active');
        
        // Animate hamburger icon
        const spans = mobileMenuToggle.querySelectorAll('span');
        if (mobileMenuToggle.classList.contains('active')) {
            spans[0].style.transform = 'rotate(45deg) translateY(8px)';
            spans[1].style.opacity = '0';
            spans[2].style.transform = 'rotate(-45deg) translateY(-8px)';
        } else {
            spans[0].style.transform = '';
            spans[1].style.opacity = '';
            spans[2].style.transform = '';
        }
    });
    
    // Close menu when clicking outside
    document.addEventListener('click', (e) => {
        if (!navMenu.contains(e.target) && !mobileMenuToggle.contains(e.target)) {
            navMenu.classList.remove('active');
            mobileMenuToggle.classList.remove('active');
            const spans = mobileMenuToggle.querySelectorAll('span');
            spans[0].style.transform = '';
            spans[1].style.opacity = '';
            spans[2].style.transform = '';
        }
    });
    
    // Close menu when clicking a link
    navMenu.querySelectorAll('a').forEach(link => {
        link.addEventListener('click', () => {
            navMenu.classList.remove('active');
            mobileMenuToggle.classList.remove('active');
            const spans = mobileMenuToggle.querySelectorAll('span');
            spans[0].style.transform = '';
            spans[1].style.opacity = '';
            spans[2].style.transform = '';
        });
    });
}

// ===========================
// Navbar Background on Scroll
// ===========================
const navbar = document.querySelector('.navbar');
let lastScrollTop = 0;

window.addEventListener('scroll', () => {
    const scrollTop = window.pageYOffset || document.documentElement.scrollTop;
    
    if (scrollTop > 50) {
        navbar.style.boxShadow = '0 4px 6px -1px rgba(0, 0, 0, 0.1)';
    } else {
        navbar.style.boxShadow = '0 1px 2px 0 rgba(0, 0, 0, 0.05)';
    }
    
    lastScrollTop = scrollTop;
});

// ===========================
// Animate on Scroll (Simple)
// ===========================
const observerOptions = {
    threshold: 0.1,
    rootMargin: '0px 0px -50px 0px'
};

const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.style.opacity = '1';
            entry.target.style.transform = 'translateY(0)';
        }
    });
}, observerOptions);

// Observe feature cards, deployment cards, etc.
document.querySelectorAll('.feature-card, .deployment-card, .doc-card, .community-card').forEach(card => {
    card.style.opacity = '0';
    card.style.transform = 'translateY(30px)';
    card.style.transition = 'opacity 0.6s ease, transform 0.6s ease';
    observer.observe(card);
});

// ===========================
// Scroll Progress Bar
// ===========================
const progressBar = document.querySelector('.scroll-progress-bar');

function updateScrollProgress() {
    const scrollTop = window.pageYOffset || document.documentElement.scrollTop;
    const scrollHeight = document.documentElement.scrollHeight - document.documentElement.clientHeight;
    const scrollPercentage = (scrollTop / scrollHeight) * 100;
    
    if (progressBar) {
        progressBar.style.width = scrollPercentage + '%';
    }
}

window.addEventListener('scroll', updateScrollProgress);
updateScrollProgress(); // Call once on load

// ===========================
// Active Navigation Link
// ===========================
const sections = document.querySelectorAll('section[id]');
const navLinks = document.querySelectorAll('.nav-menu a[href^="#"]');

function updateActiveNavLink() {
    let current = '';
    const scrollPosition = window.pageYOffset + 100;
    
    sections.forEach(section => {
        const sectionTop = section.offsetTop;
        const sectionHeight = section.clientHeight;
        
        if (scrollPosition >= sectionTop && scrollPosition < sectionTop + sectionHeight) {
            current = section.getAttribute('id');
        }
    });
    
    navLinks.forEach(link => {
        link.classList.remove('active');
        const href = link.getAttribute('href');
        if (href === `#${current}`) {
            link.classList.add('active');
        }
    });
}

window.addEventListener('scroll', updateActiveNavLink);
updateActiveNavLink(); // Call once on load

// ===========================
// Table Responsive Wrapper
// ===========================
document.querySelectorAll('table').forEach(table => {
    if (!table.parentElement.classList.contains('comparison-table-wrapper') &&
        !table.parentElement.classList.contains('benchmark-table-wrapper')) {
        const wrapper = document.createElement('div');
        wrapper.style.overflowX = 'auto';
        table.parentNode.insertBefore(wrapper, table);
        wrapper.appendChild(table);
    }
});

// ===========================
// Lazy Load Images (if any)
// ===========================
if ('loading' in HTMLImageElement.prototype) {
    const images = document.querySelectorAll('img[loading="lazy"]');
    images.forEach(img => {
        img.src = img.dataset.src;
    });
} else {
    // Fallback for older browsers
    const script = document.createElement('script');
    script.src = 'https://cdnjs.cloudflare.com/ajax/libs/lazysizes/5.3.2/lazysizes.min.js';
    document.body.appendChild(script);
}

// ===========================
// Performance Monitoring
// ===========================
if ('PerformanceObserver' in window) {
    try {
        const perfObserver = new PerformanceObserver((list) => {
            for (const entry of list.getEntries()) {
                if (entry.entryType === 'navigation') {
                    console.log('Page Load Time:', entry.loadEventEnd - entry.loadEventStart, 'ms');
                }
            }
        });
        perfObserver.observe({ entryTypes: ['navigation'] });
    } catch (e) {
        // Silently fail if PerformanceObserver is not supported
    }
}

// ===========================
// External Links in New Tab
// ===========================
document.querySelectorAll('a[href^="http"]').forEach(link => {
    if (!link.hasAttribute('target')) {
        link.setAttribute('target', '_blank');
        link.setAttribute('rel', 'noopener noreferrer');
    }
});

// ===========================
// Keyboard Navigation
// ===========================
document.addEventListener('keydown', (e) => {
    // Escape key closes mobile menu
    if (e.key === 'Escape' && navMenu && navMenu.classList.contains('active')) {
        navMenu.classList.remove('active');
        mobileMenuToggle.classList.remove('active');
    }
    
    // Ctrl/Cmd + K for search (placeholder)
    if ((e.ctrlKey || e.metaKey) && e.key === 'k') {
        e.preventDefault();
        console.log('Search functionality can be implemented here');
    }
});

// ===========================
// Dark Mode Toggle (Optional)
// ===========================
const createDarkModeToggle = () => {
    const toggle = document.createElement('button');
    toggle.className = 'dark-mode-toggle';
    toggle.innerHTML = '🌙';
    toggle.setAttribute('aria-label', 'Toggle dark mode');
    toggle.style.cssText = `
        position: fixed;
        bottom: 100px;
        right: 32px;
        width: 50px;
        height: 50px;
        border-radius: 50%;
        border: none;
        background: #4f46e5;
        color: white;
        font-size: 1.5rem;
        cursor: pointer;
        box-shadow: 0 10px 15px -3px rgba(0, 0, 0, 0.1);
        transition: all 0.3s ease;
        z-index: 998;
    `;
    
    toggle.addEventListener('click', () => {
        document.body.classList.toggle('dark-mode');
        const isDark = document.body.classList.contains('dark-mode');
        toggle.innerHTML = isDark ? '☀️' : '🌙';
        localStorage.setItem('darkMode', isDark);
    });
    
    // Check saved preference
    if (localStorage.getItem('darkMode') === 'true') {
        document.body.classList.add('dark-mode');
        toggle.innerHTML = '☀️';
    }
    
    document.body.appendChild(toggle);
};

// Uncomment to enable dark mode toggle
// createDarkModeToggle();

// ===========================
// Console Welcome Message
// ===========================
console.log('%c🔷 LatticeDB Wiki', 'font-size: 24px; font-weight: bold; color: #4f46e5;');
console.log('%cWelcome to LatticeDB - Next-Gen RDBMS', 'font-size: 14px; color: #6b7280;');
console.log('%cGitHub: https://github.com/hoangsonww/LatticeDB-DBMS', 'font-size: 12px; color: #4f46e5;');
console.log('%cContributions welcome!', 'font-size: 12px; color: #10b981;');

// ===========================
// Analytics (Placeholder)
// ===========================
const trackEvent = (eventName, eventData = {}) => {
    // Placeholder for analytics tracking
    if (window.gtag) {
        window.gtag('event', eventName, eventData);
    }
    console.log('Event tracked:', eventName, eventData);
};

// Track button clicks
document.querySelectorAll('.btn, .community-link').forEach(button => {
    button.addEventListener('click', (e) => {
        trackEvent('button_click', {
            button_text: e.target.textContent.trim(),
            button_href: e.target.href || 'no-href'
        });
    });
});

// Track tab changes
tabButtons.forEach(button => {
    button.addEventListener('click', (e) => {
        trackEvent('tab_change', {
            tab_name: e.target.getAttribute('data-tab')
        });
    });
});

// ===========================
// Service Worker Registration
// ===========================
if ('serviceWorker' in navigator && location.protocol === 'https:') {
    window.addEventListener('load', () => {
        // Uncomment to enable service worker
        // navigator.serviceWorker.register('/sw.js')
        //     .then(registration => console.log('SW registered:', registration))
        //     .catch(err => console.log('SW registration failed:', err));
    });
}

// ===========================
// Prevent Flash of Unstyled Content
// ===========================
document.addEventListener('DOMContentLoaded', () => {
    document.body.style.visibility = 'visible';
});

// ===========================
// Error Handling
// ===========================
window.addEventListener('error', (e) => {
    console.error('Global error caught:', e.error);
});

window.addEventListener('unhandledrejection', (e) => {
    console.error('Unhandled promise rejection:', e.reason);
});

// ===========================
// Page Load Complete
// ===========================
window.addEventListener('load', () => {
    console.log('✅ Page fully loaded');
    
    // Re-initialize Mermaid for any dynamically added diagrams
    setTimeout(() => {
        if (typeof mermaid !== 'undefined') {
            mermaid.init(undefined, document.querySelectorAll('.mermaid'));
        }
    }, 100);
});

// ===========================
// Utility Functions
// ===========================

/**
 * Debounce function to limit rate of function calls
 */
function debounce(func, wait) {
    let timeout;
    return function executedFunction(...args) {
        const later = () => {
            clearTimeout(timeout);
            func(...args);
        };
        clearTimeout(timeout);
        timeout = setTimeout(later, wait);
    };
}

/**
 * Throttle function to limit rate of function calls
 */
function throttle(func, limit) {
    let inThrottle;
    return function(...args) {
        if (!inThrottle) {
            func.apply(this, args);
            inThrottle = true;
            setTimeout(() => inThrottle = false, limit);
        }
    };
}

/**
 * Check if element is in viewport
 */
function isInViewport(element) {
    const rect = element.getBoundingClientRect();
    return (
        rect.top >= 0 &&
        rect.left >= 0 &&
        rect.bottom <= (window.innerHeight || document.documentElement.clientHeight) &&
        rect.right <= (window.innerWidth || document.documentElement.clientWidth)
    );
}

/**
 * Get scroll percentage
 */
function getScrollPercentage() {
    const scrollTop = window.pageYOffset || document.documentElement.scrollTop;
    const scrollHeight = document.documentElement.scrollHeight - document.documentElement.clientHeight;
    return (scrollTop / scrollHeight) * 100;
}

// ===========================
// Export for external use
// ===========================
window.LatticeDB = {
    trackEvent,
    debounce,
    throttle,
    isInViewport,
    getScrollPercentage
};
