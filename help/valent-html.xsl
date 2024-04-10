<?xml version='1.0' encoding='UTF-8'?>

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
<!-- SPDX-FileCopyrightText: Andy Holmes <andrew.g.r.holmes@gmail.com> -->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                xmlns:exsl="http://exslt.org/common"
                xmlns:str="http://exslt.org/strings"
                xmlns:mal="http://projectmallard.org/1.0/"
                xmlns:subs="http://projectmallard.org/experimental/subs/"
                xmlns="http://www.w3.org/1999/xhtml"
                extension-element-prefixes="exsl"
                exclude-result-prefixes="mal str subs"
                version="1.0">

  <xsl:param name="color.bg" select="'var(--color-bg)'"/>
  <xsl:param name="color.bg.blue" select="'var(--color-bg-blue)'"/>
  <xsl:param name="color.bg.dark" select="'var(--color-bg-dark)'"/>
  <xsl:param name="color.bg.gray" select="'var(--color-bg-gray)'"/>
  <xsl:param name="color.bg.green" select="'var(--color-bg-green)'"/>
  <xsl:param name="color.bg.orange" select="'var(--color-bg-orange)'"/>
  <xsl:param name="color.bg.purple" select="'var(--color-bg-purple)'"/>
  <xsl:param name="color.bg.red" select="'var(--color-bg-red)'"/>
  <xsl:param name="color.bg.yellow" select="'var(--color-bg-yellow)'"/>

  <xsl:param name="color.fg" select="'var(--color-fg)'"/>
  <xsl:param name="color.fg.blue" select="'var(--color-fg-blue)'"/>
  <xsl:param name="color.fg.dark" select="'var(--color-fg-dark)'"/>
  <xsl:param name="color.fg.gray" select="'var(--color-fg-gray)'"/>
  <xsl:param name="color.fg.green" select="'var(--color-fg-green)'"/>
  <xsl:param name="color.fg.orange" select="'var(--color-fg-orange)'"/>
  <xsl:param name="color.fg.purple" select="'var(--color-fg-purple)'"/>
  <xsl:param name="color.fg.red" select="'var(--color-fg-red)'"/>
  <xsl:param name="color.fg.yellow" select="'var(--color-fg-yellow)'"/>

  <xsl:param name="color.blue" select="'var(--color-blue)'"/>
  <xsl:param name="color.gray" select="'var(--color-gray)'"/>
  <xsl:param name="color.green" select="'var(--color-green)'"/>
  <xsl:param name="color.orange" select="'var(--color-orange)'"/>
  <xsl:param name="color.purple" select="'var(--color-purple)'"/>
  <xsl:param name="color.red" select="'var(--color-red)'"/>
  <xsl:param name="color.yellow" select="'var(--color-yellow)'"/>

  <!-- HEAD -->
  <xsl:template name="html.head.custom">
    <xsl:param name="node" select="."/>
    <xsl:variable name="title">
      <xsl:choose>
        <xsl:when test="$node/mal:info/mal:title[@type = 'text']">
          <xsl:value-of select="normalize-space($node/mal:info/mal:title[@type = 'text'][1])"/>
        </xsl:when>
        <xsl:when test="$node/mal:info/mal:title[not(@type)]">
          <xsl:value-of select="normalize-space($node/mal:info/mal:title[not(@type)][1])"/>
        </xsl:when>
        <xsl:when test="$node/mal:title">
          <xsl:value-of select="normalize-space($node/mal:title[1])"/>
        </xsl:when>
      </xsl:choose>
    </xsl:variable>
    <xsl:variable name="description">
      <xsl:value-of select="normalize-space($node/mal:info/mal:desc[1])"/>
    </xsl:variable>

    <!-- Meta -->
    <meta name="title" content="{$title}"/>
    <meta name="description" content="{$description}"/>

    <!-- Open Graph -->
    <meta property="og:type" content="article"/>
    <meta property="og:title" content="{$title}"/>
    <meta property="og:description" content="{$description}"/>
  </xsl:template>

  <!-- CSS -->
  <xsl:template name="html.css.custom">
    <xsl:text>
/*
 * Valent CSS Overrides
 */

<!-- Theme Colors -->
:root {
  <!-- GNOME HIG Palette -->
  --blue1: rgb(153,193,241);
  --blue2: rgb(98,160,234);
  --blue3: rgb(53,132,228);
  --blue4: rgb(28,113,216);
  --blue5: rgb(26,95,180);
  --green1: rgb(143,240,164);
  --green2: rgb(87,227,137);
  --green3: rgb(51,209,122);
  --green4: rgb(46,194,126);
  --green5: rgb(38,162,105);
  --yellow1: rgb(249,240,107);
  --yellow2: rgb(248,228,92);
  --yellow3: rgb(246,211,45);
  --yellow4: rgb(245,194,17);
  --yellow5: rgb(229,165,10);
  --orange1: rgb(255,190,111);
  --orange2: rgb(255,163,72);
  --orange3: rgb(255,120,0);
  --orange4: rgb(230,97,0);
  --orange5: rgb(198,70,0);
  --red1: rgb(246,97,81);
  --red2: rgb(237,51,59);
  --red3: rgb(224,27,36);
  --red4: rgb(192,28,40);
  --red5: rgb(165,29,45);
  --purple1: rgb(220,138,221);
  --purple2: rgb(192,97,203);
  --purple3: rgb(145,65,172);
  --purple4: rgb(129,61,156);
  --purple5: rgb(97,53,131);
  --brown1: rgb(205,171,143);
  --brown2: rgb(181,131,90);
  --brown3: rgb(152,106,68);
  --brown4: rgb(134,94,60);
  --brown5: rgb(99,69,44);
  --light1: rgb(255,255,255);
  --light2: rgb(246,245,244);
  --light3: rgb(222,221,218);
  --light4: rgb(192,191,188);
  --light5: rgb(154,153,150);
  --dark1: rgb(119,118,123);
  --dark2: rgb(94,92,100);
  --dark3: rgb(61,56,70);
  --dark4: rgb(36,31,49);
  --dark5: rgb(0,0,0);

  <!-- Adwaita Dark -->
  --adw-card-bg-dark: rgb(54,54,54);
  --adw-card-bg-dark: rgba(255, 255, 255, 0.08);
  --adw-headerbar-bg-dark: rgb(48,48,48);
  --adw-popup-bg-dark: rgb(56,56,56);
  --adw-view-bg-dark: rgb(30,30,30);
  --adw-window-bg-dark: rgb(36, 36, 36);

  <!-- Yelp Colors (Light Theme) -->
  --color-bg: var(--light1);
  --color-bg-blue: var(--blue2);
  --color-bg-dark: var(--light3);
  --color-bg-gray: var(--light2);
  --color-bg-green: var(--green2);
  --color-bg-orange: var(--orange2);
  --color-bg-purple: var(--purple2);
  --color-bg-red: var(--red2);
  --color-bg-yellow: var(--yellow2);

  --color-fg: var(--dark4);
  --color-fg-blue: var(--blue4);
  --color-fg-dark: var(--dark2);
  --color-fg-gray: var(--dark1);
  --color-fg-green: var(--green5);
  --color-fg-orange: var(--orange5);
  --color-fg-purple: var(--purple5);
  --color-fg-red: var(--red5);
  --color-fg-yellow: var(--yellow5);

  --color-blue: var(--blue3);
  --color-gray: var(--light3);
  --color-green: var(--green3);
  --color-orange: var(--orange3);
  --color-purple: var(--purple3);
  --color-red: var(--red3);
  --color-yellow: var(--yellow3);
}

<!-- Dark/Light Theme -->
@media (prefers-color-scheme: dark) {
  :root {
    /* --color-bg: var(--dark4); */
    --color-bg: var(--adw-window-bg-dark);
    --color-bg-blue: var(--blue4);
    --color-bg-dark: var(--dark2);
    /* --color-bg-gray: var(--dark3); */
    --color-bg-gray: var(--adw-card-bg-dark);
    --color-bg-green: var(--green4);
    --color-bg-orange: var(--orange4);
    --color-bg-purple: var(--purple4);
    --color-bg-red: var(--red4);
    --color-bg-yellow: var(--yellow4);

    --color-fg: var(--light2);
    --color-fg-blue: var(--blue2);
    --color-fg-dark: var(--light4);
    --color-fg-gray: var(--light4);
    --color-fg-green: var(--green1);
    --color-fg-orange: var(--orange1);
    --color-fg-purple: var(--purple1);
    --color-fg-red: var(--red1);
    --color-fg-yellow: var(--yellow1);

    --color-gray: var(--light4);
  }

  a:visited,
  a.trail:hover {
    color: var(--blue2);
  }

  #valent-banner {
    /* background-color: var(--dark3); */
    background-color: var(--adw-headerbar-bg-dark);
  }
}

@media (prefers-color-scheme: light) {
  :root {
    --color-bg: var(--light1);
    --color-bg-blue: var(--blue2);
    --color-bg-dark: var(--light3);
    --color-bg-gray: var(--light2);
    --color-bg-green: var(--green2);
    --color-bg-orange: var(--orange2);
    --color-bg-purple: var(--purple2);
    --color-bg-red: var(--red2);
    --color-bg-yellow: var(--yellow2);

    --color-fg: var(--dark4);
    --color-fg-blue: var(--blue4);
    --color-fg-dark: var(--dark2);
    --color-fg-gray: var(--dark1);
    --color-fg-green: var(--green5);
    --color-fg-orange: var(--orange5);
    --color-fg-purple: var(--purple5);
    --color-fg-red: var(--red5);
    --color-fg-yellow: var(--yellow5);

    --color-gray: var(--light4);
  }

  a:visited,
  a.trail:hover {
    color: var(--blue4);
  }

  #valent-banner {
    background-color: var(--light3);
  }
}

html, body {
  font-size: 16px;
  font-family: "Cantarell", -apple-system, BlinkMacSystemFont, "Helvetica", sans-serif;
}

@media (min-width: 700px) {
  html, body {
    font-size: 18px;
  }
}

a.linkdiv:hover {
  background-color: rgba(53,132,228, 0.1); <!-- blue3 -->
}

a.trail {
  color: currentColor;
}


<!-- Custom Elements -->
#valent-banner {
  padding: 2rem 0;
  margin-bottom: 1em;
  text-align: center;
}

#valent-banner a {
  display: flex;
  gap: 0.5rem;
  justify-content: center;
  align-items: center;

  <!-- margin: 0 auto; -->
  padding-right: 2rem;

  font-size: 1.5rem;
  font-weight: bold;
}

#valent-banner a,
#valent-banner a:hover,
#valent-banner a:visited {
  border: none;
  color: currentColor;
  text-decoration: none;
}

#valent-banner svg {
  fill: currentColor;
  height: auto;
  width: 1em;
}


footer {
  /* background-color: var(--dark4); */
  background-color: var(--adw-view-bg-dark);
  color: white;
  padding: 2rem 0;
  font-size: 80%;
  text-align: center;
}

footer a {
  color: var(--blue2) !important;
  text-decoration: none;
}

footer a:hover {
  text-decoration: underline;
}
    </xsl:text>
  </xsl:template>

  <!-- Valent Banner -->
  <xsl:template name="html.top.custom">
    <div id="valent-banner">
      <a href="{$mal.link.default_root}{$mal.link.extension}">
        <svg  version="1.1" width="16" height="16" viewBox="0 0 16 16">
          <g>
            <path d="M4 1C2.34 1 1 2.34 1 4v1h2V4c0-.555.445-1 1-1h8c.555 0 1 .445 1 1v6c0 .555-.445 1-1 1H9v2h3c1.66 0 3-1.34 3-3V4c0-1.66-1.34-3-3-3zm5 13.023V16h3c1 0 1-1 1-1s-.05-.844-4-.977zm0 0"/>
            <path d="M2.281 6C1.571 6 1 6.57 1 7.281v7.438C1 15.429 1.57 16 2.281 16H6.72C7.429 16 8 15.43 8 14.719V7.28C8 6.571 7.43 6 6.719 6zm.13 1H3c0 .555.445 1 1 1h1c.555 0 1-.445 1-1h.59a.41.41 0 0 1 .41.41v6.164a.41.41 0 0 1-.41.41H2.41a.41.41 0 0 1-.41-.41V7.41A.41.41 0 0 1 2.41 7zm0 0"/>
          </g>
        </svg>
        Valent
      </a>
    </div>
  </xsl:template>

  <!-- Header -->
  <xsl:template mode="html.header.mode" match="mal:page">
    <xsl:call-template name="mal2html.page.linktrails">
      <xsl:with-param name="node" select="."/>
    </xsl:call-template>
  </xsl:template>


  <!-- Footer -->
  <xsl:template mode="html.footer.mode" match="mal:page">
    <xsl:for-each select="/mal:page/mal:info/mal:credit[mal:years]">
      <p>
        <xsl:text>© </xsl:text>
        <xsl:value-of select="mal:years[1]"/>
        <xsl:text> </xsl:text>
        <xsl:value-of select="mal:name[1]"/>
      </p>
    </xsl:for-each>

    <xsl:variable name="license" select="/mal:page/mal:info/mal:license[1]"/>

    <xsl:if test="$license">
      <div class="ui-expander">
        <div class="yelp-data yelp-data-ui-expander" data-yelp-expanded="false"/>
        <div class="inner">
          <div class="title">
            <span class="title">
              <a>
                <xsl:attribute name="href">
                  <xsl:value-of select="$license/@href"/>
                </xsl:attribute>
                <xsl:choose>
                  <xsl:when test="$license/@href = 'https://creativecommons.org/licenses/by-sa/4.0/'">
                    <xsl:text>CC BY-SA 4.0</xsl:text>
                  </xsl:when>
                  <xsl:otherwise>
                    <xsl:text>License</xsl:text>
                  </xsl:otherwise>
                </xsl:choose>
              </a>
            </span>
          </div>
          <div class="region">
            <div class="contents">
              <xsl:apply-templates mode="mal2html.block.mode" select="$license/*"/>
            </div>
          </div>
        </div>
      </div>
    </xsl:if>
  </xsl:template>

  <xsl:template mode="mal2html.block.mode"
                match="mal:media[@type='application'][@mime='application/html']">
    <xsl:copy-of select="*"/>
  </xsl:template>

  <xsl:template match="/mal:page/mal:links[@type = 'section']">
    <xsl:call-template name="mal2html.links.section"/>
  </xsl:template>
</xsl:stylesheet>
