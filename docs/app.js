(function () {
  const root = document.documentElement;

  function slugify(text) {
    return text
      .toLowerCase()
      .trim()
      .replace(/[^a-z0-9\s-]/g, "")
      .replace(/\s+/g, "-")
      .replace(/-+/g, "-");
  }

  function initTheme() {
    // Dark-only mode: clear legacy preference and enforce default theme.
    root.removeAttribute("data-theme");
    localStorage.removeItem("docs-theme");
  }

  function initCodeCopy() {
    const blocks = document.querySelectorAll("pre");
    blocks.forEach((pre) => {
      if (pre.querySelector(".copy-btn")) return;
      const button = document.createElement("button");
      button.className = "copy-btn";
      button.type = "button";
      button.textContent = "Copy";
      button.addEventListener("click", async () => {
        const target = pre.querySelector("code");
        const text = target ? target.innerText : pre.innerText;
        try {
          await navigator.clipboard.writeText(text);
          button.textContent = "Copied";
          setTimeout(() => {
            button.textContent = "Copy";
          }, 1200);
        } catch (err) {
          button.textContent = "Failed";
          setTimeout(() => {
            button.textContent = "Copy";
          }, 1200);
        }
      });
      pre.appendChild(button);
    });
  }

  function initTOC() {
    const tocList = document.getElementById("toc-list");
    const content = document.querySelector("[data-doc-content]");
    if (!tocList || !content) return;

    const headings = content.querySelectorAll("h3, h4");
    if (!headings.length) return;

    headings.forEach((heading, index) => {
      if (!heading.id) {
        heading.id = slugify(heading.textContent || `section-${index + 1}`);
      }
      const item = document.createElement("li");
      item.className =
        heading.tagName.toLowerCase() === "h4" ? "toc-sub" : "toc-main";
      const link = document.createElement("a");
      link.href = `#${heading.id}`;
      link.textContent = heading.textContent || "Section";
      item.appendChild(link);
      tocList.appendChild(item);
    });
  }

  function initActiveNav() {
    const navLinks = document.querySelectorAll(".nav-links a");
    const current = window.location.pathname.split("/").pop() || "index.html";
    const currentHash = window.location.hash || "#overview";

    navLinks.forEach((link) => {
      const href = link.getAttribute("href") || "";
      if (href.startsWith("#") && current === "index.html") {
        if (currentHash === href) {
          link.classList.add("active");
        }
      } else if (!href.startsWith("#")) {
        const [page, hash] = href.split("#");
        if (page === current && hash) {
          if (`#${hash}` === currentHash) {
            link.classList.add("active");
          }
        } else if (page === current && !hash) {
          link.classList.add("active");
        }
      }
    });
  }

  function initSearch() {
    const input = document.getElementById("doc-search");
    const list = document.getElementById("search-results");
    if (!input || !list) return;

    fetch("./search-index.json")
      .then((res) => res.json())
      .then((docs) => {
        const render = (query) => {
          const q = query.trim().toLowerCase();
          list.innerHTML = "";
          if (!q) return;

          const hits = docs
            .filter((item) => {
              const haystack =
                `${item.title} ${item.summary} ${item.keywords.join(" ")}`.toLowerCase();
              return haystack.includes(q);
            })
            .slice(0, 8);

          if (!hits.length) {
            const none = document.createElement("li");
            none.textContent = "No matching documentation page found.";
            list.appendChild(none);
            return;
          }

          hits.forEach((hit) => {
            const li = document.createElement("li");
            const a = document.createElement("a");
            a.href = hit.url;
            a.innerHTML = `<strong>${hit.title}</strong><span>${hit.summary}</span>`;
            li.appendChild(a);
            list.appendChild(li);
          });
        };

        input.addEventListener("input", (e) => render(e.target.value));
      })
      .catch(() => {
        list.innerHTML = "<li>Search index unavailable right now.</li>";
      });
  }

  function initHighlight() {
    if (window.hljs) {
      window.hljs.highlightAll();
    }
  }

  initTheme();
  initCodeCopy();
  initTOC();
  initActiveNav();
  initSearch();
  initHighlight();
})();
