// This is what changes body from hidden
(function($) {
	if ($(window).width() >= 800) {
		$('html, body').animate({scrollTop:20}, 800);
	}
})(jQuery);

$(document).ready(function() {
   
    $('a[href=#top]').click(function(){
		if ($(window).width() >= 800) {
			$('html, body').animate({scrollTop:0}, 400);
			$('html, body').animate({scrollTop:20}, 400);
		}
		else {
			$('html, body').animate({scrollTop:0}, 400);
		}
				console.log("Javascript Works! :D")
        return false;
    });
    $('a[href=#about]').click(function(){
		jump('#about');
        return false;
    });
    $('a[href=#research]').click(function(){
		jump('#research');
        return false;
    });
    $('a[href=#development]').click(function(){
		jump('#development');
        return false;
    });
    $('a[href=#teaching]').click(function(){
		jump('#teaching');
        return false;
    });
    $('a[href=#honours]').click(function(){
		jump('#honours');
        return false;
    });
    $('a[href=#links]').click(function(){
		jump('#links');
        return false;
    });

});

function jump(id) {
	var position = $(id).position();
	var curr_pos = $(document).scrollTop();

	var page_height = $(document).height();
	var window_height = $(window).height();

	var bottom = page_height - window_height - 50;
	var bottom_bounce = page_height - window_height - 10;

	if (position.top > bottom) {
		$('html, body').animate({scrollTop:bottom_bounce}, 400);
		$('html, body').animate({scrollTop:bottom}, 400);
	}
	else {
		if (curr_pos + 48 >= position.top) {
			$('html, body').animate({scrollTop:position.top - 78}, 400);
			$('html, body').animate({scrollTop:position.top - 48}, 400);
		}
		else {
			$('html, body').animate({scrollTop:position.top - 18}, 400);
			$('html, body').animate({scrollTop:position.top - 48}, 400);
		}
	}
}

