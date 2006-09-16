<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY % html "IGNORE">
<![%html;[
<!ENTITY % print "IGNORE">
<!ENTITY docbook.dsl PUBLIC "-//Norman Walsh//DOCUMENT DocBook HTML Stylesheet//EN" CDATA dsssl>
]]>
<!ENTITY % print "INCLUDE">
<![%print;[
<!ENTITY docbook.dsl PUBLIC "-//Norman Walsh//DOCUMENT DocBook Print Stylesheet//EN" CDATA dsssl>
]]>
]>

<style-sheet>

<style-specification id="print" use="docbook">
<style-specification-body> 

;; PRINT

(define %body-font-family% "Palatino")
(define %mono-font-family% "Courier")
(define %admon-font-family% "Helvetica")
(define %title-font-family% "Helvetica")

; fix pdf rendering problem with glossdiv
(element glossdiv (process-children))
(element (glossdiv title) ($lowtitle$ 2 2))

(define %division-title-quadding% 'start)
(define %division-subtitle-quadding% 'start)

(define %refentry-new-page%
  ;; REFENTRY refentry-new-page
  ;; PURP 'RefEntry' starts on new page?
  ;; DESC
  ;; If true, each 'RefEntry' begins on a new page.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(element (formalpara title)
  ;; Change the way Formal Paragraph titles are displayed.
  ;;
  ;; borrowed from:
  ;;   The GNOME Documentation Project's
  ;;   Custom DocBook Stylesheet Layer
  ;;   by Dave Mason dcm@redhat.com
  ($lowtitle$ 5 7))

(define %default-title-end-punct%
  ;; REFENTRY default-title-end-punct
  ;; PURP Default punctuation at the end of a run-in head.
  ;; DESC
  ;; The punctuation used at the end of a run-in head (e.g. on FORMALPARA).
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  ":")

(element link
  ;; Wrap link text with " " on page #"
  ;; 
  ;; No warnings about missing targets.  Jade will do that for us, and
  ;; this way we can use -wno-idref if we really don't care.
  (let* ((endterm   (attribute-string (normalize "endterm")))
         (linkend   (attribute-string (normalize "linkend")))
         (target    (element-with-id linkend))
         (link-cont (make sequence (literal "\"") (process-children) (literal "\" on page ") (element-page-number-sosofo target))))
    (if (node-list-empty? target)
        link-cont
        (make link
          destination: (node-list-address target)
          link-cont))))

(define %two-side%
  ;; REFENTRY two-side
  ;; PURP Is two-sided output being produced?
  ;; DESC
  ;; If '%two-side%' is true, headers and footers are alternated
  ;; so that the "outer" and "inner" headers will be correctly
  ;; placed in the bound document.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(define %footnote-ulinks%
  ;; REFENTRY footnote-ulinks
  ;; PURP Generate footnotes for ULinks?
  ;; DESC
  ;; If true, the URL of each ULink will appear as a footnote.
  ;; Processing ULinks this way may be very, very slow. It requires
  ;; walking over every descendant of every component in order to count
  ;; both ulinks and footnotes.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #f)

(define %show-comments%
  ;; REFENTRY show-comments
  ;; PURP Display Comment elements?
  ;; DESC
  ;; If true, comments will be displayed, otherwise they are suppressed.
  ;; Comments here refers to the 'Comment' element, which will be renamed
  ;; 'Remark' in DocBook V4.0, not SGML/XML comments which are unavailable.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(define ($object-titles-after$)
  ;; REFENTRY object-titles-after
  ;; PURP List of objects who's titles go after the object
  ;; DESC
  ;; Titles of formal objects (Figures, Equations, Tables, etc.)
  ;; in this list will be placed below the object instead of above it.
  ;;
  ;; This is a list of element names, for example:
  ;; '(list (normalize "figure") (normalize "table"))'.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  (list (normalize "figure") (normalize "table")))

(define %hsize-bump-factor%
  ;; REFENTRY hsize-bump-factor
  ;; PURP Font scaling factor
  ;; DESC
  ;; Internally, the stylesheet refers to font sizes in purely relative
  ;; terms. This is done by defining a scaled set of fonts
  ;; (sizes 1, 2, 3, etc.)
  ;; based at the default text font size (e.g. 10pt). The '%hsize-bump-factor%'
  ;; describes the ratio between scaled sizes. The default is 1.2.
  ;;
  ;; Each hsize is '%hsize-bump-factor%' times larger than
  ;; the previous hsize. For example, if the base size is 10pt, and
  ;; '%hsize-bump-factor%'
  ;; 1.2, hsize 1 is 12pt, hsize 2 is 14.4pt, hsize 3 is 17.28pt, etc.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  1.1)

(define %paper-type%
  ;; REFENTRY paper-type
  ;; PURP Name of paper type
  ;; DESC
  ;; The paper type value identifies the sort of paper in use, for example,
  ;; 'A4' or 'USletter'. Setting the paper type is an
  ;; easy shortcut for setting the correct paper height and width.
  ;; 
  ;; See %page-width% and %page-height concerning what other page size
  ;; are available.  Some common examples are 'A4', 'USletter',
  ;; 'A4landscape', 'USlandscape'.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  ;; "A4"
  "USletter")

</style-specification-body>
</style-specification>

<style-specification id="html" use="docbook">
<style-specification-body> 

;; HTML

(define %html-ext% 
  ;; REFENTRY html-ext
  ;; PURP Default extension for HTML output files
  ;; DESC
  ;; The default extension for HTML output files.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  ".html")

(define %show-comments%
  ;; REFENTRY show-comments
  ;; PURP Display Comment elements?
  ;; DESC
  ;; If true, comments will be displayed, otherwise they are suppressed.
  ;; Comments here refers to the 'Comment' element, which will be renamed
  ;; 'Remark' in DocBook V4.0, not SGML/XML comments which are unavailable.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #t)

(define (chunk-element-list)
  (list (normalize "preface")
        (normalize "chapter")
        (normalize "appendix")
        (normalize "article")
        (normalize "glossary")
        (normalize "bibliography")
        (normalize "index")
        (normalize "colophon")
        (normalize "setindex")
        (normalize "reference")
        (normalize "refentry")
        (normalize "part")
        (normalize "book") ;; just in case nothing else matches...
        (normalize "set")  ;; sets are definitely chunks...
        ))

(define (chunk? #!optional (nd (current-node)))
  ;; 1. The (sgml-root-element) is always a chunk.
  ;; 2. If nochunks is #t or the dbhtml PI on the root element
  ;;    specifies chunk='no', then the root element is the only
  ;;    chunk.
  ;; 3. Otherwise, elements in the chunk-element-list are chunks
  ;;    unless they're combined with their parent.
  ;; 4. Except for bibliographys, which are only chunks if they
  ;;    occur in book or article.
  ;;
  (let* ((notchunk (or (and (equal? (gi nd) (normalize "bibliography"))
                            (not (or (equal? (gi (parent nd)) (normalize "book"))
                                     (equal? (gi (parent nd)) (normalize "article")))))))
         (maybechunk (not notchunk)))
    (if (node-list=? nd (sgml-root-element))
        #t
        (if (or nochunks
                (equal? (dbhtml-value (sgml-root-element) "chunk") "no"))
            #f
            (if (member (gi nd) (chunk-element-list))
                (if (combined-chunk? nd)
                    #f
                    maybechunk)
                #f)))))

(define $generate-chapter-toc$
  ;; REFENTRY generate-chapter-toc
  ;; PURP Should a Chapter Table of Contents be produced?
  ;; DESC
  ;; If true, an automatically generated
  ;; chapter TOC should be included. By default, it's true.  It's false if
  ;; the output is going to a single file and the current node isn't the
  ;; root element.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  (lambda ()
    #f))

(define ($object-titles-after$)
  ;; REFENTRY object-titles-after
  ;; PURP List of objects who's titles go after the object
  ;; DESC
  ;; Titles of formal objects (Figures, Equations, Tables, etc.)
  ;; in this list will be placed below the object instead of above it.
  ;;
  ;; This is a list of element names, for example:
  ;; '(list (normalize "figure") (normalize "table"))'.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  (list (normalize "figure") (normalize "table")))

(define %spacing-paras%
  ;; REFENTRY spacing-paras
  ;; PURP Block-element spacing hack
  ;; DESC
  ;; Should extraneous "P" tags be output to force the correct vertical
  ;; spacing around things like tables.  This is ugly because different
  ;; browsers do different things.  Turning this one can also create
  ;; illegal HTML.
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  #f)

(define %default-title-end-punct%
  ;; REFENTRY default-title-end-punct
  ;; PURP Default punctuation at the end of a run-in head.
  ;; DESC 
  ;; The punctuation used at the end of a run-in head (e.g. on FORMALPARA).
  ;; /DESC
  ;; AUTHOR N/A
  ;; /REFENTRY
  ":")

(define (build-toc nd depth #!optional (chapter-toc? #f) (first? #t))
  (let ((toclist (toc-list-filter
                  (node-list-filter-by-gi (children nd)
                                          (append (division-element-list)
                                                  (component-element-list)
                                                  (section-element-list)))))
        (wrappergi (if first? "DIV" "DD"))
        (wrapperattr (if first? '(("CLASS" "TOC")) '())))
    (if (or (<= depth 0)
            (node-list-empty? toclist)
            (and chapter-toc?
                 (not %force-chapter-toc%)
                 (<= (node-list-length toclist) 1)))
        (empty-sosofo)
        (make element gi: wrappergi
              attributes: wrapperattr
              (make element gi: "DL"
                    (if first?
                        (make element gi: "DT"
                              (make element gi: "B"
                                    (literal (gentext-element-name (normalize "toc")))))
                        (empty-sosofo))
                    (let loop ((nl toclist))
                      (if (node-list-empty? nl)
                          (empty-sosofo)
                          (sosofo-append
                            (toc-entry (node-list-first nl))
                            (build-toc (node-list-first nl)
                                       (- depth 1) chapter-toc? #f)
                            (loop (node-list-rest nl))))))(make empty-element gi: "BR")))))

</style-specification-body>
</style-specification>

<external-specification id="docbook" document="docbook.dsl">

</style-sheet>
